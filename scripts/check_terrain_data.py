#!/usr/bin/env python3
"""Check terrain startup compatibility and tooltip bounds with production objects.

Build first (portable by default). All templates and raw caches are copied to
temporary fixtures; installed saves, configuration, and caches are never opened.
"""
import argparse
import os
from pathlib import Path
import re
import shlex
import shutil
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "scripts/output/terrain-data-check"

HARNESS = r'''
#include "angband.h"
#include "externs.h"
#include "init.h"
#include "init/init2-internal.h"
#include "log/log.h"
#include <assert.h>
#include <stdio.h>

bool sdl_object_tooltip_feature_name(int y, int x, cptr* name);
bool __wrap_sdl_mouse_feature_known_for_action(int y, int x)
{ (void)y; (void)x; return true; }

static void tooltips(void)
{
    cptr name;
    p_ptr->cur_map_hgt = p_ptr->cur_map_wid = 24;
    cave_feat = calloc(MAX_DUNGEON_HGT, sizeof(*cave_feat));
    cave_info = calloc(MAX_DUNGEON_HGT, sizeof(*cave_info));
    cave_rewired = calloc(MAX_DUNGEON_HGT, sizeof(*cave_rewired));
    assert(cave_feat && cave_info && cave_rewired);
    for (int f = FEAT_BRIDGE_HEAD; f <= FEAT_BRIDGE_TAIL; ++f)
    {
        cave_feat[10][10] = f;
        assert(sdl_object_tooltip_feature_name(10, 10, &name));
        assert(strstr(name, "bridge"));
    }
    cave_feat[10][10] = FEAT_OPEN;
    assert(sdl_object_tooltip_feature_name(10, 10, &name));
    assert(!strcmp(name, "open door"));
    /* Reproduce the short table in the old 88-record installation. */
    feature_type* original = f_info;
    unsigned count = z_info->f_max;
    z_info->f_max = FEAT_BRIDGE_HEAD;
    f_info = calloc(z_info->f_max, sizeof(*f_info));
    assert(f_info);
    cave_feat[10][10] = FEAT_BRIDGE_POISON_H;
    assert(!sdl_object_tooltip_feature_name(10, 10, &name) && !name);
    cave_feat[10][10] = FEAT_OPEN;
    f_info[FEAT_OPEN].mimic = 255;
    assert(!sdl_object_tooltip_feature_name(10, 10, &name) && !name);
    free(f_info); f_info = original; z_info->f_max = count;
    cave_feat[10][10] = 255;
    assert(!sdl_object_tooltip_feature_name(10, 10, &name) && !name);
    assert(!sdl_object_tooltip_feature_name(-1, 10, &name));
    assert(!sdl_object_tooltip_feature_name(24, 10, &name));
    free(cave_feat); free(cave_info); free(cave_rewired);
}

int main(int argc, char** argv)
{
    assert(argc == 4);
    setbuf(stdout, NULL);
    log_set_level(LOG_ERROR);
    assert(SDL_Init(0));
    ANGBAND_DIR_EDIT = argv[1];
    ANGBAND_DIR_DATA = argv[2];
    assert(!init_z_info());
    errr result = init_f_info();
    int expected = atoi(argv[3]);
    printf("Terrain load: features=%u result=%d expected=%d\n",
        (unsigned)z_info->f_max, result, expected);
    assert(result == expected);
    if (!result) tooltips();
    SDL_Quit();
    return 0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", default="build-portable")
    parser.add_argument("--deployed", type=Path)
    args = parser.parse_args()
    build = ROOT / args.build
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    cmake = build / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects
                                  if not p.endswith("/src/main.c.obj")), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(build / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin",
        env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
                    "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
                    "-Wl,--wrap=sdl_mouse_feature_known_for_action", "-o", str(exe)],
                   cwd=build, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="fixtures-", dir=OUT) as temporary:
        root = Path(temporary)
        assert root.resolve().is_relative_to(OUT.resolve())

        def fixture(label, assets=ROOT / "lib/edit"):
            directory = root / label
            edit, data = directory / "edit", directory / "data"
            edit.mkdir(parents=True)
            data.mkdir()
            for name in ("limits.txt", "terrain.txt"):
                shutil.copy2(assets / name, edit / name)
            return edit, data

        def run(edit, data, expected):
            result = subprocess.run([str(exe), str(edit), str(data), str(expected)],
                                    cwd=data, env=env, capture_output=True,
                                    text=True, timeout=30)
            assert result.returncode == 0, result.stdout + result.stderr

        edit, data = fixture("current")
        run(edit, data, 0)
        raw = data / "terrain.raw"
        good = raw.read_bytes()
        stamp = raw.stat().st_mtime_ns
        run(edit, data, 0)
        assert raw.read_bytes() == good and raw.stat().st_mtime_ns == stamp
        print("Current templates and unchanged raw-cache reload; bridge names and tooltip bounds: PASS")

        edit, data = fixture("old-limits")
        limits = edit / "limits.txt"
        limits.write_text(re.sub(r"(?m)^M:F:\d+", "M:F:88", limits.read_text()), encoding="utf-8")
        run(edit, data, 2)  # PARSE_ERROR_OBSOLETE_FILE
        run(edit, data, 2)
        print("Old 88-feature limits rejected with fresh and cached limits: PASS")

        edit, data = fixture("missing-terrain")
        terrain = edit / "terrain.txt"
        terrain.write_text(terrain.read_text().split("N:88:", 1)[0], encoding="utf-8")
        run(edit, data, 8)  # PARSE_ERROR_OUT_OF_BOUNDS
        run(edit, data, 8)
        print("Updated limits with old terrain rejected with fresh and cached terrain: PASS")

        edit, data = fixture("bad-cache")
        head_size = struct.unpack_from("<I", good, 8)[0]
        name_size = struct.unpack_from("<I", good, 16)[0]
        for field, value in (("name", name_size), ("mimic", 255), ("name", 0)):
            bad = bytearray(good)
            struct.pack_into("<I" if field == "name" else "<B", bad,
                             head_size + 94 * 16 + (0 if field == "name" else 8), value)
            (data / "terrain.raw").write_bytes(bad)
            run(edit, data, 8)
        print("Invalid cached names, missing records, and mimic indices rejected: PASS")

        if args.deployed:
            edit, data = fixture("deployed", args.deployed / "lib/edit")
            run(edit, data, 0)
            run(edit, data, 0)
            print("Deployed terrain templates: fresh and cached startup PASS")


if __name__ == "__main__":
    main()

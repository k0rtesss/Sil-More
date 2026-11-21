#!/usr/bin/env python3
"""Check Final Look reveal, inspection and frozen state in an isolated engine.

Build with build-cmake.bat standard first. Uses production engine objects and
the complete spectator entry/exit path, with only pane painting/input replaced.
--baseline recompiles the owning HEAD sources to demonstrate the regressions.
All writable paths stay in a disposable directory; player files are untouched.
"""
from pathlib import Path
import argparse
import json
import os
import shlex
import subprocess
import tempfile

import check_new_monsters as engine

ROOT, BUILD = engine.ROOT, engine.BUILD
OUT = ROOT / "scripts/output/final-look"
OWNERS = [
    "src/dungeon/dungeon-spectator.c",
    "src/dungeon/dungeon-narrative.c",
    "src/cave/cave-view.c",
    "src/object/object-knowledge.c",
    "src/monster/monster-update.c",
    "src/cave/cave-environment.c",
    "src/achievements.c",
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", action="store_true")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    prefix, body = engine.HARNESS.split("int main(int argc,char** argv)", 1)
    setup = body.split("    check_templates();", 1)[0]
    checks = (ROOT / "scripts/tests/final-look-test.c").read_text(encoding="utf-8")
    source = OUT / "check.c"
    source.write_text(prefix + checks + "int main(int argc,char** argv)" + setup
                      + "    return check_final_look();\n}\n", encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    extras = []
    excluded = ["/src/main.c.obj"]
    if args.baseline:
        for path in OWNERS:
            baseline = OUT / (Path(path).stem + "-baseline.c")
            baseline.write_bytes(subprocess.check_output(
                ["git", "show", "HEAD:" + path], cwd=ROOT))
            extras.append(str(baseline))
            excluded.append("/" + path + ".obj")
    # Expose the actual SDL visibility/focus helpers, without mocking their rules.
    for path, exports in [
        ("src/sdl/render/sdl-idle-animation.c",
         "int fixture_liquid(int y,int x){return visible_liquid(y,x);}\n"
         "int fixture_fixture(int y,int x){return visible_fixture(y,x);}\n"),
        ("src/sdl/render/sdl-material-edge.c",
         "bool fixture_material(int y,int x){return material_edge_cell_visible(y,x);}\n"),
        ("src/sdl/render/sdl-chasm-transition.c",
         "bool fixture_chasm(int y,int x){return chasm_known(y,x);}\n"),
        ("src/sdl/input/sdl-screen-pointer.c",
         "bool fixture_focus(int y,int x){g_minimap_pending_focus_active=false;"
         "sdl_minimap_focus(y,x);return g_minimap_pending_focus_active;}\n"),
    ]:
        wrapper = OUT / (Path(path).stem + "-fixture.c")
        wrapper.write_text('#include "' + (ROOT / path).as_posix() + '"\n' + exports,
                           encoding="utf-8")
        extras.append(str(wrapper))
        excluded.append("/" + path + ".obj")
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects
                                  if not obj.endswith(tuple(excluded))), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / ("baseline.exe" if args.baseline else "check.exe")
    wraps = ("handle_stuff", "request_command", "msg_print", "inkey",
             "sdl_display_pixel_map", "sdl_minimap_begin", "sdl_minimap_end")
    subprocess.run([
        "C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
        "-I" + str(ROOT / "src/dungeon"), "-I" + str(ROOT / "src/cave"),
        "-I" + str(ROOT / "src/monster"),
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), *extras,
        "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
        *("-Wl,--wrap=" + name for name in wraps), "-o", str(exe)],
        cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="profile-", dir=OUT) as profile:
        run = subprocess.run([str(exe), str(ROOT / "lib/edit"), profile], cwd=profile,
                             env=env, capture_output=True, text=True, timeout=45)
    print(run.stdout, end="")
    print(run.stderr, end="")
    receipt = {"baseline": args.baseline, "returncode": run.returncode,
               "stdout": run.stdout, "stderr": run.stderr}
    (OUT / ("baseline.json" if args.baseline else "summary.json")).write_text(
        json.dumps(receipt, indent=2), encoding="utf-8")
    raise SystemExit(run.returncode)


if __name__ == "__main__":
    main()

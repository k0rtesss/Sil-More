#!/usr/bin/env python3
"""Exercise the production lore layer and app-setting JSON without player data."""
from pathlib import Path
import json
import os
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/generic-lore"


def function(source, name):
    match = re.search(r"^(?:static )?(?:void|bool)\s+" + name + r"\(", source, re.M)
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


def main():
    catalog = json.loads((ROOT / "lib/pref/generic_lore.json").read_text(encoding="utf-8"))
    for group in ("terms", "passages"):
        seen = {}
        for source, target in catalog[group]:
            assert source and target and source != target
            key = source.lower() if group == "terms" else source
            assert key not in seen or seen[key] == target, (group, source)
            seen[key] = target
    passages = dict(catalog["passages"])
    stories = []
    for line in (ROOT / "lib/edit/story.txt").read_text(encoding="utf-8-sig").splitlines():
        if line.startswith("N:"):
            stories.append("")
        elif line.startswith("D:"):
            stories[-1] += line[2:]
    assert all(story in passages for story in stories if story), "Every story needs original prose"
    print(f"Original story coverage: {len(stories)} chapters PASS", flush=True)
    OUT.mkdir(parents=True, exist_ok=True)
    config = (ROOT / "src/sdl-config.c").read_text(encoding="utf-8")
    groups = re.findall(r"static const byte app_\w+_options\[\] = \{.*?\};", config, re.S)
    helpers = [function(config, name) for name in (
        "option_list_contains", "option_is_retired_app_text_option",
        "option_is_app_persistent", "sdl_config_default_app_bool",
        "sdl_config_load_app_option_group", "sdl_config_save_app_option_group")]
    (OUT / "app-options-under-test.inc").write_text(
        "\n".join(groups + helpers), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = ";".join([
        str(BUILD / "_deps/SDL"), "C:/msys64/mingw64/bin",
        "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "generic-lore-test.exe"
    subprocess.run([
        "C:/msys64/mingw64/bin/cc.exe", "-std=c17", "-DUSE_SDL", "-UNDEBUG",
        "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
        "@CMakeFiles/sil-more.dir/includes_C.rsp",
        "-I", str(OUT),
        str(ROOT / "scripts/tests/generic-lore-test.c"), str(ROOT / "src/lore.c"),
        str(ROOT / "src/cJSON.c"), str(ROOT / "src/tables.c"),
        str(BUILD / "_deps/SDL/libSDL3.dll.a"), "-o", str(exe)
    ], cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe), str(ROOT / "lib/pref")], cwd=OUT, env=env, check=True)
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    response = OUT / "engine-objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects
        if not obj.endswith("/src/main.c.obj")), encoding="utf-8")
    env["PATH"] = ";".join([
        *(str(BUILD / "_deps" / dep) for dep in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        env["PATH"]])
    runtime = OUT / "generic-lore-runtime-test.exe"
    subprocess.run([
        "C:/msys64/mingw64/bin/cc.exe", "-std=c17", "-DUSE_SDL", "-UNDEBUG",
        "@CMakeFiles/sil-more.dir/includes_C.rsp",
        str(ROOT / "scripts/tests/generic-lore-runtime-test.c"), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp",
        "-Wl,--wrap=inkey,--wrap=save_pane_config_to_json", "-o", str(runtime)
    ], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="engine-", dir=OUT) as state:
        subprocess.run([str(runtime), str(ROOT / "lib/edit"), state],
            cwd=state, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()

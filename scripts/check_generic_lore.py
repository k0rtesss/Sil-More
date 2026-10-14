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

TEXT_FIELDS = {
    "ability.txt": ("N", "DE"), "monster.txt": ("N", "D"),
    "object.txt": ("N", "D"), "special.txt": ("N", "D"),
    "artefact.txt": ("N", "D"), "race.txt": ("N", "D"),
    "character.txt": ("N", "D"), "history.txt": ("N", "D"),
    "story.txt": ("N", "D"), "curses.txt": ("N", "DEPH"),
    "blessing.txt": ("N", "SDM"), "oath.txt": ("O", "TDPRFUCM EQZ".replace(" ", "")),
    "quest.txt": ("Q", "TCIW"), "vault.txt": ("N", "MH"),
}


def load_catalogs():
    primary = json.loads((ROOT / "lib/pref/generic_lore.json").read_text(encoding="utf-8"))
    catalogs = [primary] + [json.loads((ROOT / "lib/pref" / name).read_text(encoding="utf-8"))
                           for name in primary.get("includes", [])]
    merged = {"terms": {}, "passages": {}, "descriptions": {}}
    for catalog in catalogs:
        for group in merged:
            for pair in catalog.get(group, []):
                source, target = pair[:2]
                assert len(pair) == 2 or (group == "terms" and len(pair) == 3 and isinstance(pair[2], bool))
                assert source and target and source != target
                if group == "descriptions" and source in merged[group]:
                    assert merged[group][source] == target, ("Conflicting full fields", source)
                merged[group][source] = target
    # Whole-field descriptions take priority over old substring passages.
    for source in merged["passages"]:
        if source in merged["descriptions"]:
            merged["passages"][source] = merged["descriptions"][source]
    return {group: [list(pair) for pair in values.items()] for group, values in merged.items()}


def authored_fields():
    for filename, (marker, fields) in TEXT_FIELDS.items():
        current = None
        rows = []
        for line in (ROOT / "lib/edit" / filename).read_text(encoding="utf-8-sig").splitlines():
            if line.startswith(marker + ":"):
                if current is not None:
                    rows.append(current)
                current = {"record": line.split(":")[1]}
            elif current is not None and len(line) >= 2 and line[1] == ":" and line[0] in fields:
                current.setdefault(line[0], []).append(line[2:])
        if current is not None:
            rows.append(current)
        for row in rows:
            for field in fields:
                text = ("\n" if filename == "quest.txt" and field in "IW" else "").join(row.get(field, []))
                if text:
                    yield filename, row["record"], field, text
    for number, line in enumerate((ROOT / "lib/edit/style.txt").read_text(encoding="utf-8-sig").splitlines(), 1):
        if line.startswith(("M1:", "M2:")):
            yield "style.txt", number, line[:2], line[3:].strip()
    for number, line in enumerate((ROOT / "lib/edit/skeleton_note.txt").read_text(encoding="utf-8-sig").splitlines(), 1):
        if line.startswith(("O:", "C:", "M:")):
            payload = line.split(":", 4 if line[0] == "M" else 3)[-1]
            for part in payload.split("||"):
                yield "skeleton_note.txt", number, line[0], part.strip()


def check_full_coverage(catalog):
    replacements = dict(catalog["passages"])
    replacements.update(catalog["descriptions"])
    counts = {}
    missing = []
    for filename, record, field, source in authored_fields():
        counts[filename] = counts.get(filename, 0) + 1
        if source not in replacements or source == replacements[source]:
            missing.append((filename, record, field, source[:100]))
    assert not missing, ("Missing complete descriptions (renaming does not count)", missing)
    for name, count in counts.items():
        print(f"Full prose coverage: {name}: {count}/{count} PASS", flush=True)

    # Tutorials are largely mechanical. Require full rewrites for every step
    # containing a theme term, and for the related oath/quest steps authored in
    # the help catalog, rather than accepting a successful noun replacement.
    help_catalog = json.loads((ROOT / "lib/pref/generic_lore_help.json").read_text(encoding="utf-8"))
    help_fields = dict(help_catalog["descriptions"])
    terms = re.compile(r"(?<![A-Za-z])(?:" + "|".join(
        re.escape(source) for source, _ in catalog["terms"]) + r")(?![A-Za-z])", re.I)
    tutorials = json.loads((ROOT / "lib/help/tutorials.json").read_text(encoding="utf-8"))
    covered = 0
    for lesson in tutorials["lessons"]:
        for index, step in enumerate(lesson["steps"]):
            source = step["text"]
            if source in help_fields or terms.search(source):
                assert source in help_fields, ("Missing full tutorial rewrite", lesson["id"], index, source)
                covered += 1
    print(f"Full prose coverage: tutorials.json: {covered}/{covered} PASS", flush=True)
    # Catalog keys must still be actual help/tutorial strings after edits.
    help_source = (ROOT / "src/ui/help.c").read_text(encoding="utf-8")
    literals = set(re.findall(r'"((?:\\.|[^"\\])*)"', help_source))
    tutorial_fields = {step["text"] for lesson in tutorials["lessons"] for step in lesson["steps"]}
    for source in help_fields:
        assert source in tutorial_fields or source in literals, ("Stale help rewrite", source)


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
    catalog = load_catalogs()
    for group in ("terms", "passages", "descriptions"):
        seen = {}
        for source, target in catalog[group]:
            assert source and target and source != target
            key = source.lower() if group == "terms" else source
            assert key not in seen or seen[key] == target, (group, source)
            seen[key] = target
    check_full_coverage(catalog)
    passages = dict(catalog["passages"])
    passages.update(catalog["descriptions"])
    stories = []
    for line in (ROOT / "lib/edit/story.txt").read_text(encoding="utf-8-sig").splitlines():
        if line.startswith("N:"):
            stories.append("")
        elif line.startswith("D:"):
            stories[-1] += line[2:]
    assert all(story in passages for story in stories if story), "Every story needs original prose"
    print(f"Original story coverage: {len(stories)} chapters PASS", flush=True)
    OUT.mkdir(parents=True, exist_ok=True)
    merged = OUT / "merged-catalog-under-test.json"
    merged.write_text(json.dumps(catalog, ensure_ascii=False), encoding="utf-8")
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
    subprocess.run([str(exe), str(ROOT / "lib/pref"), str(merged)], cwd=OUT, env=env, check=True)
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    response = OUT / "engine-objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects
        if not obj.endswith(("/src/main.c.obj", "/src/cmd/ui/cmd-ui-abilities.c.obj",
                             "/src/object/object-info.c.obj", "/src/ui/help.c.obj",
                             "/src/tutorial/tutorial.c.obj"))), encoding="utf-8")
    env["PATH"] = ";".join([
        *(str(BUILD / "_deps" / dep) for dep in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        env["PATH"]])
    runtime = OUT / "generic-lore-runtime-test.exe"
    fixture = (ROOT / "scripts/tests/generic-lore-runtime-test.c").read_text(encoding="utf-8")
    start = fixture.index("static term test_term;")
    frontend = []
    # Internal headers also include the legacy, unguarded externs.h. Keep their
    # declarations while removing that second include in the combined fixture.
    metarun_header = (ROOT / "src/metarun/metarun-internal.h").read_text(encoding="utf-8")
    metarun_header = metarun_header.replace('#include "../metarun.h"', '#include "metarun.h"')
    metarun_header = metarun_header.replace('#include "externs.h"\n', "")
    for name in ("src/cmd/ui/cmd-ui-abilities.c", "src/object/object-info.c",
                 "src/ui/help.c", "src/tutorial/tutorial.c"):
        source = (ROOT / name).read_text(encoding="utf-8").replace('#include "externs.h"\n', "")
        frontend.append(source.replace('#include "metarun/metarun-internal.h"', metarun_header))
    generated = OUT / "generic-ui-runtime.c"
    generated.write_text(fixture[:start] + "\n".join(frontend) + "\n"
        + (ROOT / "scripts/tests/generic-lore-ui-checks.inc").read_text(encoding="utf-8")
        + fixture[start:], encoding="utf-8")
    subprocess.run([
        "C:/msys64/mingw64/bin/cc.exe", "-std=c17", "-DUSE_SDL", "-UNDEBUG",
        "@CMakeFiles/sil-more.dir/includes_C.rsp",
        "-I", str(ROOT / "src/tutorial"),
        str(generated), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp",
        "-Wl,--wrap=inkey,--wrap=save_pane_config_to_json", "-o", str(runtime)
    ], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="engine-", dir=OUT) as state:
        subprocess.run([str(runtime), str(ROOT / "lib/edit"), state],
            cwd=state, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()

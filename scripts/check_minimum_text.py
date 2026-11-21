#!/usr/bin/env python3
"""Exercise minimum-text ability and item UI with real engine data.

Requires build-standard. Uses temporary data and config paths, never player saves.
"""
from pathlib import Path
import os
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/minimum-text"


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    catalog = (ROOT / "lib/edit/object-minimum.txt").read_text(encoding="utf-8-sig")
    records = []
    for line in catalog.splitlines():
        if not line or line.startswith("#"):
            continue
        match = re.fullmatch(r"([KAE]):(\d+):(.+)", line)
        assert match, line
        key = (match[1], int(match[2]))
        assert key not in records, ("Duplicate short description", key)
        records.append(key)
    for kind, filename in (("K", "object.txt"), ("E", "special.txt"), ("A", "artefact.txt")):
        ids = {int(m[1]) for m in re.finditer(r"(?m)^N:(\d+):", (ROOT / "lib/edit" / filename).read_text(encoding="utf-8"))}
        assert all(index in ids for entry_kind, index in records if entry_kind == kind)
    # The compact item catalogue may remove lore, but may not paraphrase or
    # truncate the gameplay paragraphs retained from the source templates.
    item_descriptions = {}
    for chunk in re.split(r"(?m)^N:", (ROOT / "lib/edit/object.txt").read_text(encoding="utf-8"))[1:]:
        lines = chunk.splitlines()
        match = re.match(r"(\d+):", lines[0])
        if match:
            raw = "".join(line[2:] for line in lines if line.startswith("D:"))
            item_descriptions[int(match[1])] = raw.replace("conditions.If", "conditions. If").replace("conditions.It", "conditions. It")
    for line in catalog.splitlines():
        match = re.fullmatch(r"K:(\d+):(.+)", line)
        if match:
            assert item_descriptions[int(match[1])].strip().endswith(match[2]), ("Gameplay text changed", match[1])
    special_descriptions = {}
    for chunk in re.split(r"(?m)^N:", (ROOT / "lib/edit/special.txt").read_text(encoding="utf-8"))[1:]:
        lines = chunk.splitlines()
        match = re.match(r"(\d+):", lines[0])
        if match:
            special_descriptions[int(match[1])] = "".join(line[2:] for line in lines if line.startswith("D:"))
    short_specials = {}
    for line in catalog.splitlines():
        match = re.fullmatch(r"E:(\d+):(.+)", line)
        if match:
            short_specials[int(match[1])] = match[2]
            assert len(match[2]) <= 140, ("Special description too long", match[1])
    assert set(short_specials) == {index for index, text in special_descriptions.items() if text.strip()}, (
        "Special descriptions missing or obsolete",
        sorted(set(special_descriptions) - set(short_specials)),
        sorted(set(short_specials) - set(special_descriptions)),
    )
    # Preserve the explicit special-item rules, rather than abbreviating them.
    assert special_descriptions[181].endswith(short_specials[181])
    light_armour_rule = "One who bears it can still weave and turn as freely as the unarmoured."
    assert light_armour_rule in special_descriptions[185] and light_armour_rule in short_specials[185]
    monster_source = (ROOT / "lib/edit/monster.txt").read_text(encoding="utf-8")
    monster_with_lore = set()
    for chunk in re.split(r"(?m)^N:", monster_source)[1:]:
        lines = chunk.splitlines()
        match = re.match(r"(\d+):", lines[0])
        if match and any(line.startswith("D:") and line[2:].strip() for line in lines):
            monster_with_lore.add(int(match[1]))
    short_monsters = {}
    for line in (ROOT / "lib/edit/monster-minimum.txt").read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("#"): continue
        match = re.fullmatch(r"R:(\d+):(.+)", line)
        assert match and int(match[1]) not in short_monsters, line
        assert len(match[2]) <= 140, line
        short_monsters[int(match[1])] = match[2]
    assert set(short_monsters) == monster_with_lore
    quest_records = re.findall(r"(?m)^Q:(\d+):(.+)$", (ROOT / "lib/edit/quest-minimum.txt").read_text(encoding="utf-8"))
    assert {int(index) for index, _ in quest_records} == set(range(1, 7)) and len(quest_records) == 6
    fixture = (ROOT / "scripts/tests/generic-lore-runtime-test.c").read_text(encoding="utf-8")
    prefix = fixture[:fixture.index("int main(int argc, char** argv)")]
    setup = fixture[fixture.index("int main(int argc, char** argv)"):]
    setup = setup[:setup.index("    u64b random_state;")]
    checks = (ROOT / "scripts/tests/generic-lore-ui-checks.inc").read_text(encoding="utf-8")
    helpers = checks[checks.index("static void generic_normalize"):checks.index("static void check_generic_ui")]
    minimum = checks[checks.index("/* Exercise real description builders"):]
    metarun_header = (ROOT / "src/metarun/metarun-internal.h").read_text(encoding="utf-8")
    metarun_header = metarun_header.replace('#include "../metarun.h"', '#include "metarun.h"')
    metarun_header = metarun_header.replace('#include "externs.h"\n', "")
    frontend = []
    for name in ("src/cmd/ui/cmd-ui-abilities.c", "src/object/object-info.c"):
        source = (ROOT / name).read_text(encoding="utf-8").replace('#include "externs.h"\n', "")
        frontend.append(source.replace('#include "metarun/metarun-internal.h"', metarun_header))
    source = OUT / "check.c"
    source.write_text(fixture[:fixture.index("static term test_term;")] + "\n".join(frontend)
        + helpers + minimum + prefix[fixture.index("static term test_term;"):]
        + setup + r'''    if (SDL_getenv("MINIMUM_TEST_MISSING_CATALOG")) {
        object_type item; char description[8192];
        ANGBAND_DIR_EDIT = argv[2];
        op_ptr->opt[OPT_minimum_text_mode] = true;
        assert(lore_set_generic(false));
        object_prep(&item, 344);
        cptr text = object_lore_select_base_text(&item, description, sizeof(description));
        assert(text && strstr(text, "draught laced with venom"));
        k_info[344].aware = true;
        text_out_hook = generic_capture_output;
        generic_object_output[0] = '\0';
        screen_out_head(&item);
        assert(strstr(generic_object_output, "draught laced with venom"));
        quest_minimum_summary(QUEST_ID_AULE, false, description, sizeof(description));
        assert(strstr(description, "At Aulë's forge") && strstr(description, "Reward:"));
        generic_object_output[0] = '\0';
        describe_monster(51, false, NULL);
        assert(strstr(generic_object_output, "Speed:") && strstr(generic_object_output, "Resists:"));
        puts("Missing short catalogues: item/quest rules and monster traits remain visible PASS");
    } else {
        check_minimum_text_ui();
        check_minimum_quests_and_monsters();
    }
    lore_cleanup(); term_nuke(&test_term); SDL_Quit(); return 0;
}
''', encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects
        if not obj.endswith(("/src/main.c.obj", "/src/cmd/ui/cmd-ui-abilities.c.obj",
                             "/src/object/object-info.c.obj"))), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / dep) for dep in
        ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-std=c17", "-DUSE_SDL", "-UNDEBUG",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp",
        "-Wl,--wrap=inkey,--wrap=save_pane_config_to_json", "-o", str(exe)],
        cwd=BUILD, env=env, check=True)
    for missing in (False, True):
        with tempfile.TemporaryDirectory(prefix="engine-", dir=OUT) as state:
            run_env = env.copy()
            if missing:
                run_env["MINIMUM_TEST_MISSING_CATALOG"] = "1"
            subprocess.run([str(exe), str(ROOT / "lib/edit"), state],
                           cwd=state, env=run_env, check=True, timeout=30)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Check blessing/curse lore separation with production engine units.

Requires a configured Windows build. All writable paths use a disposable
profile. --baseline recompiles the changed HEAD sources to show the bug.
"""
from pathlib import Path
import argparse
import json
import os
import shlex
import subprocess
import tempfile

from check_new_monsters import HARNESS as ENGINE
from check_monster_scent_save import fixture_function
from check_disease import function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/blessing-knowledge"
CASES = ("inline", "menu", "blitz", "visibility", "discovery", "persistence")

WRAPPERS = {
    "src/metarun/metarun-blessing-ui.c": r'''
bool fixture_receive(void) { return blessing_gain_minor(NULL, 0, NULL); }
bool fixture_lift(void) { return blessing_remove_curse(NULL, 0, NULL); }
''',
    "src/birth/birth-blitz.c": r'''
void fixture_blitz_pick(int id, bool blessing) { blitz_apply_effect_pick(id, blessing); }
void fixture_blitz_preview(int id, bool blessing, bool descriptions) {
    blitz_add_effect_details(id, blessing, descriptions);
}
''',
    "src/metarun/metarun-effects-ui.c": r'''
int fixture_effect(int id) {
    Term_clear();
    int stacks = CURSE_GET(id);
    int rows = metarun_render_active_effect_block(id, stacks, 0, 100);
    assert(rows == metarun_active_effect_block_lines(id, stacks, 100));
    return rows;
}
''',
    "src/cmd/ui/cmd-ui-character.c": r'''
void fixture_hint(char *out, size_t size, u32b flag) {
    out[0] = '\0'; character_sheet_append_curse_hint(out, size, flag);
}
''',
    "src/metarun/metarun-stats-ui.c": r'''
int fixture_chronicle(int *ids) {
    return story_book_sdl_collect_known_curses(ids, METAR_CURSE_SLOTS);
}
''',
    "src/cmd/ui/cmd-ui-knowledge.c": r'''
int fixture_catalogue(int *ids) { return knowledge_collect_curses(ids); }
''',
    "src/tutorial/tutorial-world.c": r'''
void fixture_tutorial_counts(int *curses, int *blessings) {
    world_snapshot state = snapshot(); *curses = state.curse; *blessings = state.blessing;
}
''',
}
CHANGED = {
    "src/metarun/metarun-blessing-ui.c", "src/birth/birth-blitz.c",
    "src/metarun/metarun-effects-ui.c", "src/cmd/ui/cmd-ui-character.c",
    "src/tutorial/tutorial-world.c",
}
COLLECTORS = {
    "src/metarun/metarun-stats-ui.c": (
        "story_book_curse_seen", "story_book_sdl_collect_known_curses"),
    "src/cmd/ui/cmd-ui-knowledge.c": ("knowledge_collect_curses",),
}

TESTS = r'''
#include "metarun/metarun-internal.h"
bool fixture_receive(void);
bool fixture_lift(void);
void fixture_blitz_pick(int, bool);
void fixture_blitz_preview(int, bool, bool);
int fixture_effect(int);
void fixture_hint(char *, size_t, u32b);
int fixture_chronicle(int *);
int fixture_catalogue(int *);
void fixture_tutorial_counts(int *, int *);

static int checks;
static char menu_key = '\r';
static bool capture_blitz_preview;
static char preview[4096];
#define CHECK(t) do { checks++; if (!(t)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #t); exit(1); } } while (0)
char __wrap_metarun_inkey_hidden(void) { return menu_key; }
void __real_sdl_character_sheet_screen_add_select_detail(cptr, int, cptr);
void __wrap_sdl_character_sheet_screen_add_select_detail(cptr text, int attr, cptr desc) {
    if (!capture_blitz_preview) {
        __real_sdl_character_sheet_screen_add_select_detail(text, attr, desc);
        return;
    }
    SDL_strlcat(preview, text, sizeof(preview));
    SDL_strlcat(preview, "\n", sizeof(preview));
}

static void fresh(void) {
    run_mode_reset();
    reset_defaults(&metar);
    metar.id = 8701;
    memset(metar.curse_stacks, 0, sizeof(metar.curse_stacks));
    metar.curses_seen = 0;
    metar.major_blessings = 0;
    metar.blessing_points = 100;
    metar.blessing_points_spent = 0;
    metaruns[0] = metar;
    menu_key = '\r';
    Rand_state_init(8701);
}

static void offer(int id) {
    metar.pending_blessing_count = 1;
    memset(metar.pending_blessing_choices, 255, sizeof(metar.pending_blessing_choices));
    metar.pending_blessing_choices[0] = id;
}

static void check_lists(int expected, int id) {
    int ids[METAR_CURSE_SLOTS];
    CHECK(fixture_chronicle(ids) == expected);
    if (expected) CHECK(ids[0] == id);
    CHECK(fixture_catalogue(ids) == expected);
    if (expected) CHECK(ids[0] == id);
}

static bool screen_has(cptr text) {
    char line[101];
    for (int y=0; y<40; y++) {
        for (int x=0; x<100; x++) {
            byte attr; char glyph;
            CHECK(Term_what(x, y, &attr, &glyph) == 0);
            line[x] = glyph;
        }
        line[100] = '\0';
        if (strstr(line, text)) return true;
    }
    return false;
}

static void check_inline(void) {
    int gifts=0;
    CHECK(strstr(blessing_display_name(2), "Blooming Vigor"));
    CHECK(strstr(curse_display_name(2), "Withering Blossoms"));
    for (int id=0; id<z_info->cu_max; id++) {
        if (!cu_info[id].blessing_name) continue;
        fresh();
        int cap = CURSE_BLESSING_CAP(id);
        for (int stack=1; stack<=MAX(cap, 1); stack++) {
            offer(id);
            CHECK(metarun_inline_choose_minor_blessing(id));
            CHECK(CURSE_GET(id) == -stack);
            CHECK(!CURSE_SEEN(id));
            CHECK(metar.blessing_points_spent == stack);
            check_lists(0, id);
        }
        if (cap) {
            offer(id);
            CHECK(!metarun_inline_choose_minor_blessing(id));
            CHECK(CURSE_GET(id) == -cap && !CURSE_SEEN(id));
        }
        gifts++;
    }
    CHECK(gifts > 20);
    fresh(); offer(2); metar.blessing_points = 0;
    CHECK(!metarun_inline_choose_minor_blessing(2));
    CHECK(!CURSE_GET(2) && !CURSE_SEEN(2));
    printf("Inline: %d blessing templates, stacking/caps and no-credit refusal PASS\n", gifts);
}

static void check_menu(void) {
    fresh(); offer(2); menu_key = ESCAPE;
    CHECK(!fixture_receive());
    CHECK(!CURSE_GET(2) && !CURSE_SEEN(2) && !metar.blessing_points_spent);
    menu_key = '\r';
    CHECK(fixture_receive());
    CHECK(CURSE_GET(2) == -1 && !CURSE_SEEN(2));
    CHECK(metar.blessing_points_spent == 1);
    check_lists(0, 2);
    fresh(); CURSE_SEEN_SET(2); offer(2);
    CHECK(fixture_receive());
    CHECK(CURSE_GET(2) == -1 && CURSE_SEEN(2));
    check_lists(1, 2);
    puts("Exchange menu: cancel, receive and preserved historical curse lore PASS");
}

static void check_blitz(void) {
    fresh(); run_mode_set_current(RUN_MODE_BLITZ);
    int ids[METAR_CURSE_SLOTS], templates=0;
    for (int id=0; id<z_info->cu_max; id++)
        if (cu_info[id].name) templates++;
    CHECK(!CURSE_SEEN(-1) && !CURSE_SEEN(METAR_CURSE_SLOTS));
    CHECK(!*blitz_runtime_curses_seen());
    CHECK(fixture_catalogue(ids) == templates);
    for (int i=0; i<templates; i++) CHECK(cu_info[ids[i]].name);
    CHECK(fixture_chronicle(ids) == 0); /* Chronicle retains the Story Tale's lore. */
    for (int id=0; id<z_info->cu_max; id++) {
        if (!cu_info[id].name) continue;
        CHECK(cu_info[id].power);
        blitz_runtime_reset();
        CURSE_SET(id, 1); /* Directly acquired/restored without a discovery bit. */
        CHECK(CURSE_SEEN(id) && !*blitz_runtime_curses_seen());
        CHECK(fixture_effect(id) > 0);
        CHECK(!screen_has("not yet identified"));
        if (cu_info[id].power)
            CHECK(screen_has("Effect:"));
        for (int descriptions=0; descriptions<=1; descriptions++) {
            preview[0] = '\0'; capture_blitz_preview = true;
            fixture_blitz_preview(id, false, descriptions);
            capture_blitz_preview = false;
            if (cu_info[id].power) {
                char expected[1024];
                strnfmt(expected, sizeof(expected), "Effect: %s",
                    lore_text(cu_text + cu_info[id].power));
                CHECK(strstr(preview, expected));
            }
            CHECK(!*blitz_runtime_curses_seen());
        }
        if (!cu_info[id].blessing_name) continue;
        blitz_runtime_reset();
        fixture_blitz_pick(id, true);
        CHECK(CURSE_GET(id) == -1 && CURSE_SEEN(id));
        CHECK(!*blitz_runtime_curses_seen());
        CHECK(fixture_catalogue(ids) == templates);
        fixture_blitz_pick(id, false); /* Opposites cancel. A curse pick is known. */
        CHECK(CURSE_GET(id) == 0 && CURSE_SEEN(id));
    }
    int8_t restored[METAR_CURSE_SLOTS] = {0};
    restored[2] = restored[4] = 1;
    blitz_runtime_restore(restored, 0);
    CHECK(CURSE_SEEN(2) && CURSE_SEEN(4) && !*blitz_runtime_curses_seen());
    fixture_effect(2);
    CHECK(screen_has("Effect: Constitution -1"));
    char hint[1024];
    fixture_hint(hint, sizeof(hint), RHF_MEL_PENALTY);
    CHECK(strstr(hint, "Known curse") && strstr(hint, "Melee Penalty"));
    CHECK(!metar.curses_seen && !metar.curse_stacks[2]);
    run_mode_set_current(RUN_MODE_STORY);
    CHECK(!CURSE_SEEN(2) && !CURSE_SEEN(4));
    check_lists(0, 2);
    run_mode_reset();
    printf("Blitz: %d curse templates, both preview modes, no-bit acquisition/restore, "
        "catalogue, active effects/hints and Story isolation PASS\n", templates);
}

static void check_visibility(void) {
    char hint[1024];
    int curses, blessings;
    fresh(); offer(2);
    CHECK(metarun_inline_choose_minor_blessing(2));
    CHECK(!CURSE_SEEN(2));
    CHECK(fixture_effect(2) > 0);
    CHECK(screen_has("Effect: Constitution +1"));
    CHECK(!screen_has("not yet identified"));
    fixture_tutorial_counts(&curses, &blessings);
    CHECK(curses == 0 && blessings == 1);
    fresh(); CURSE_SET(2, 1);
    fixture_effect(2);
    CHECK(screen_has("not yet identified"));
    CHECK(!screen_has("Effect: Constitution -1"));
    fixture_tutorial_counts(&curses, &blessings);
    CHECK(curses == 0 && blessings == 0);
    CURSE_SEEN_SET(2);
    fixture_effect(2);
    CHECK(screen_has("Effect: Constitution -1"));
    fixture_tutorial_counts(&curses, &blessings);
    CHECK(curses == 1 && blessings == 0);
    fresh(); CURSE_SET(4, -1);
    fixture_hint(hint, sizeof(hint), RHF_MEL_AFFINITY);
    CHECK(strstr(hint, "Known blessing") && strstr(hint, "Melee Affinity"));
    CHECK(!CURSE_SEEN(4));
    CURSE_SET(4, 1);
    fixture_hint(hint, sizeof(hint), RHF_MEL_PENALTY);
    CHECK(strstr(hint, "Unknown curse") && !strstr(hint, "Known curse"));
    CURSE_SEEN_SET(4);
    fixture_hint(hint, sizeof(hint), RHF_MEL_PENALTY);
    CHECK(strstr(hint, "Known curse") && strstr(hint, "Melee Penalty"));
    puts("Visibility: active effects, matched row heights, hints and tutorial counts PASS");
}

static void check_discovery(void) {
    fresh(); offer(2);
    CHECK(metarun_inline_choose_minor_blessing(2));
    for (int i=0; i<24; i++) self_knowledge();
    CHECK(!CURSE_SEEN(2));
    /* A later curse must still be discovered independently. */
    CURSE_SET(2, 1);
    for (int i=0; i<120 && !CURSE_SEEN(2); i++) self_knowledge();
    CHECK(CURSE_SEEN(2));
    check_lists(1, 2);
    CHECK(metarun_inline_remove_curse(2));
    CHECK(!CURSE_GET(2) && CURSE_SEEN(2));
    offer(2);
    CHECK(metarun_inline_choose_minor_blessing(2));
    CHECK(CURSE_GET(2) == -1 && CURSE_SEEN(2));
    check_lists(1, 2);
    fresh(); CURSE_SET(2, 1);
    CHECK(metarun_inline_remove_curse(2));
    CHECK(!CURSE_GET(2) && CURSE_SEEN(2));
    fresh(); CURSE_SET(2, 1);
    CHECK(fixture_lift());
    CHECK(!CURSE_GET(2) && CURSE_SEEN(2));
    puts("Discovery: Self Knowledge, both lifting paths and retained curse history PASS");
}

static void reload_tale(void) {
    CHECK(save_metaruns() == 0);
    mem_free_null(metaruns);
    metarun_max = 0;
    metarun_created = false;
    CHECK(load_metaruns(false) == 0);
    CHECK(metarun_max == 1 && metar.id == 8701);
}

static void check_persistence(void) {
    fresh(); offer(2);
    metar.next_blessing_seed = 123456789;
    CHECK(metarun_inline_choose_minor_blessing(2));
    reload_tale();
    CHECK(metar.next_blessing_seed == 123456789);
    CHECK(CURSE_GET(2) == -1 && !CURSE_SEEN(2));
    fixture_effect(2);
    CHECK(screen_has("Effect: Constitution +1"));
    check_lists(0, 2);
    CURSE_SEEN_SET(2);
    reload_tale();
    CHECK(CURSE_GET(2) == -1 && CURSE_SEEN(2));
    check_lists(1, 2);
    char path[1024];
    CHECK(build_meta_path(path, sizeof(path), NULL, META_RAW));
    SDL_IOStream* file = SDL_IOFromFile(path, "r+b");
    meta_file_header header;
    metarun old;
    CHECK(file && SDL_ReadIO(file, &header, sizeof(header)) == sizeof(header));
    CHECK(SDL_ReadIO(file, &old, sizeof(old)) == sizeof(old));
    header.version_extra = 10; /* Last 1.1 revision before Easy blessings. */
    old.last_played = 1750000000;
    old.next_blessing_seed = 0xDEADBEEF; /* Previously unused bytes. */
    CHECK(SDL_SeekIO(file, 0, SDL_IO_SEEK_SET) == 0);
    CHECK(SDL_WriteIO(file, &header, sizeof(header)) == sizeof(header));
    CHECK(SDL_WriteIO(file, &old, sizeof(old)) == sizeof(old) && SDL_CloseIO(file));
    mem_free_null(metaruns);
    metarun_max = 0;
    metarun_created = false;
    CHECK(load_metaruns(false) == 0 && metar.next_blessing_seed == old.last_played);
    reload_tale();
    CHECK(metar.next_blessing_seed == old.last_played && CURSE_GET(2) == -1 && CURSE_SEEN(2));
    puts("Persistence: blessing and historical curse lore survive real meta.raw roundtrip PASS");
}

static int run_checks(cptr which) {
    metarun_max = 1; current_run = 0;
    metaruns = mem_alloc_array(1, metarun);
    CHECK(metaruns);
    if (!strcmp(which,"all") || !strcmp(which,"inline")) check_inline();
    if (!strcmp(which,"all") || !strcmp(which,"menu")) check_menu();
    if (!strcmp(which,"all") || !strcmp(which,"blitz")) check_blitz();
    if (!strcmp(which,"all") || !strcmp(which,"visibility")) check_visibility();
    if (!strcmp(which,"all") || !strcmp(which,"discovery")) check_discovery();
    if (!strcmp(which,"all") || !strcmp(which,"persistence")) check_persistence();
    printf("Blessing/curse knowledge: %d checks PASS\n", checks);
    SDL_Quit(); return 0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", action="store_true")
    parser.add_argument("--case", choices=("all", *CASES), default="all")
    args = parser.parse_args()
    out = OUT / ("baseline" if args.baseline else "fixed")
    out.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE[:ENGINE.index("static const char* guids[]")]
    prefix = prefix.replace('#include "level-generation/level-generation-internal.h"', '')
    prefix = prefix.replace('#include "monster/monster-abilities.h"', '')
    setup = ENGINE[ENGINE.index("int main(int argc,char** argv)"):].split("    check_templates();", 1)[0]
    setup = setup.replace("assert(argc==3);", "assert(argc==4);")
    setup = setup.replace("&test_term,80,24,256", "&test_term,100,40,256")
    setup += r'''
    char tales[1024];
    CHECK(path_build(tales, sizeof(tales), argv[2], "metaruns"));
    CHECK(SDL_CreateDirectory(tales));
    ANGBAND_DIR_APEX = ANGBAND_DIR_SAVE = argv[2];
    ANGBAND_DIR_METARUN = tales;
'''
    source = out / "check.c"
    source.write_text(prefix + fixture_function("terminal_extra") + "\n" + TESTS
                      + setup + "    return run_checks(argv[3]);\n}\n", encoding="utf-8")
    extras = []
    for path, exports in WRAPPERS.items():
        owner = ROOT / path
        if path in COLLECTORS:
            # These filters are the only dependencies needed from the two
            # large browsers. Keep unrelated browser edits out of this fixture.
            content = ('#include "angband.h"\n#include "externs.h"\n'
                       '#include "metarun.h"\n'
                       + "\n".join(function(path, name) for name in COLLECTORS[path]))
        else:
            content = '#include "' + owner.as_posix() + '"\n'
        if args.baseline and path in CHANGED:
            owner = out / (Path(path).stem + "-baseline.c")
            owner.write_bytes(subprocess.check_output(["git", "show", "HEAD:" + path], cwd=ROOT))
            content = '#include "' + owner.as_posix() + '"\n'
            if path == "src/metarun/metarun-effects-ui.c":
                exports = exports.replace("    int stacks = CURSE_GET(id);\n", "")
                exports = exports.replace("(id, stacks,", "(id,")
        wrapper = out / (Path(path).stem + "-fixture.c")
        wrapper.write_text('#include <assert.h>\n' + content + exports,
                           encoding="utf-8")
        extras.append(str(wrapper))
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    excluded = tuple("/" + path + ".obj" for path in ("src/main.c", *WRAPPERS)
                     if path not in COLLECTORS)
    response = out / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects
                                  if not obj.endswith(excluded)), encoding="utf-8")
    env = os.environ.copy()
    env.update(SDL_VIDEO_DRIVER="dummy", SDL_AUDIO_DRIVER="dummy", SDL_RENDER_DRIVER="software")
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = out / "check.exe"
    subprocess.run([
        "C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g", "-Wall", "-Wextra",
        *("-I" + str((ROOT / path).parent) for path in WRAPPERS),
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), *extras,
        "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
        "-Wl,--wrap=metarun_inkey_hidden",
        "-Wl,--wrap=sdl_character_sheet_screen_add_select_detail", "-o", str(exe)],
        cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="profile-", dir=out) as profile:
        run = subprocess.run([str(exe), str(ROOT / "lib/edit"), profile, args.case],
                             cwd=profile, env=env, capture_output=True, text=True, timeout=60)
    print(run.stdout, end="")
    print(run.stderr, end="")
    (out / (args.case + ".json")).write_text(json.dumps({
        "baseline": args.baseline, "case": args.case, "returncode": run.returncode,
        "stdout": run.stdout, "stderr": run.stderr}, indent=2), encoding="utf-8")
    raise SystemExit(run.returncode)


if __name__ == "__main__":
    main()

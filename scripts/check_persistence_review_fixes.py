#!/usr/bin/env python3
"""Check Tale memory ownership/retention and complete savefile checksum footers.

Uses production engine objects and the current persistence source against
temporary directories only. Run after rebuilding; --portable uses the portable
build. No real saves, installation databases, or user settings are opened.
"""
from pathlib import Path
import argparse
import os
import re
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]

WRITER = r'''
#include "fs/save.c"
#include <assert.h>
size_t fixture_write_full(byte* out, size_t capacity)
{
    fff = SDL_IOFromMem(out, capacity); assert(fff);
    bool ok = wr_savefile();
    size_t size = (size_t)SDL_TellIO(fff);
    assert(SDL_CloseIO(fff)); fff = NULL; assert(ok);
    return size;
}
'''

READER = r'''
#include "fs/load.c"
#include <assert.h>
int fixture_read_full(const byte* in, size_t size)
{
    assert(size >= 4);
    fff = SDL_IOFromConstMem(in, size); assert(fff);
    xor_byte = 0; v_check = x_check = load_byte_offset = 0;
    sf_major = in[0]; sf_minor = in[1]; sf_patch = in[2]; sf_extra = in[3];
    savefile_has_varda_quest = savefile_version_at_least(0, 9, 1, 3);
    int result = rd_savefile_new_aux();
    assert(SDL_CloseIO(fff)); fff = NULL;
    return result;
}
'''

TESTS = r'''
#include "meta_state.h"
#include "metarun.h"
#include "score/score_paths.h"
#include "score/score_io.h"
#include "blitz.h"
#include "fs/path.h"
#include "fs/io_sdl.h"

size_t fixture_write_full(byte*, size_t);
int fixture_read_full(const byte*, size_t);
void reset_defaults(metarun*);
bool start_new_metarun(void);
extern metarun* metaruns;
extern s16b metarun_max, current_run;
static int checks;
#define CHECK(t) do { checks++; if (!(t)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #t); exit(1); } } while (0)
#ifndef SIL_USE_LOCAL_DATA
static int legacy_warnings;
static void count_legacy_warning(log_Event* event)
{
    if (strstr(event->fmt, "legacy memories preserved"))
        legacy_warnings++;
}
#endif

static bool count_memory(const meta_state_record_meta* meta, const void* payload,
    u32b size, void* user)
{
    CHECK(meta->metarun_id == metar.id && size == 1);
    CHECK(*(const byte*)payload == 42);
    (*(int*)user)++;
    return true;
}

static int memory_count(meta_state_db_kind kind)
{
    int count = 0;
    CHECK(meta_state_load_current_records(kind, count_memory, &count));
    return count;
}

static void check_memory_paths_and_rollover(void)
{
    const char* leaves[] = {"artefact.db", "monster.db", "dungeon.db"};
    const byte payload = 42;
    meta_state_record_meta record = {0};
    record.flags = META_STATE_RECORD_ACTIVE;
    for (int option = OPT_meta_artefact_memory; option <= OPT_meta_legendary_places; option++)
        op_ptr->opt[option] = true;

    /* A shared installation's legacy DB has no user ownership information.
     * Do not silently import it into a profile whose numeric Tale ID matches. */
#ifndef SIL_USE_LOCAL_DATA
    char legacy_path[1024];
    CHECK(path_build(legacy_path, sizeof(legacy_path), ANGBAND_DIR_APEX, "monster.db"));
    SDL_IOStream* legacy = SDL_IOFromFile(legacy_path, "wb"); CHECK(legacy);
    CHECK(SDL_WriteIO(legacy, "legacy-owner-unknown", 20) == 20);
    CHECK(SDL_CloseIO(legacy));
    CHECK(log_add_callback(count_legacy_warning, NULL, LOG_WARN) == 0);
    CHECK(meta_state_init() && legacy_warnings == 1);
#endif
    metar.id = 7001;
    for (int kind = 0; kind < META_STATE_DB_KIND_MAX; kind++) {
        char expected[1024], actual[1024];
        CHECK(score_build_meta_path(expected, sizeof(expected), leaves[kind]));
        CHECK(meta_state_build_db_path(kind, actual, sizeof(actual)));
        CHECK(!strcmp(expected, actual));
        CHECK(memory_count(kind) == 0);
        record.metarun_id = metar.id;
        CHECK(meta_state_append_current_record(kind, &record, &payload, 1));
        CHECK(memory_count(kind) == 1);
        metar.id = 7002;
        CHECK(memory_count(kind) == 0);
        record.metarun_id = metar.id;
        CHECK(meta_state_append_current_record(kind, &record, &payload, 1));
        CHECK(memory_count(kind) == 1);
        metar.id = 7001;
    }
#ifndef SIL_USE_LOCAL_DATA
    char legacy_data[20];
    legacy = SDL_IOFromFile(legacy_path, "rb"); CHECK(legacy);
    CHECK(SDL_GetIOSize(legacy) == 20);
    CHECK(SDL_ReadIO(legacy, legacy_data, sizeof(legacy_data)) == sizeof(legacy_data));
    CHECK(!memcmp(legacy_data, "legacy-owner-unknown", sizeof(legacy_data)));
    CHECK(SDL_CloseIO(legacy));
#endif

    /* Exercise the real automatic rollover, including its score-ledger
     * transaction. Tale A is inactive and unfinished when Tale B ends. */
    metarun_max = 2; current_run = 1;
    metaruns = mem_alloc_array(2, metarun); CHECK(metaruns);
    reset_defaults(&metaruns[0]); metaruns[0].id = 7001;
    reset_defaults(&metaruns[1]); metaruns[1].id = 7002;
    metar = metaruns[1];
    char scores[1024];
    char malformed[1024], preserved[3];
    CHECK(score_build_meta_path(malformed, sizeof(malformed), "malformed.raw"));
    SDL_IOStream* bad = SDL_IOFromFile(malformed, "wb"); CHECK(bad);
    CHECK(SDL_WriteIO(bad, "bad", 3) == 3 && SDL_CloseIO(bad));
    log_set_quiet(true);
    CHECK(!score_file_open(malformed, O_RDWR | O_CREAT));
    log_set_quiet(false);
    bad = SDL_IOFromFile(malformed, "rb"); CHECK(bad);
    CHECK(SDL_GetIOSize(bad) == 3);
    CHECK(SDL_ReadIO(bad, preserved, 3) == 3 && !memcmp(preserved, "bad", 3));
    CHECK(SDL_CloseIO(bad));
    CHECK(score_build_meta_path(scores, sizeof(scores), "scores.raw"));
    SDL_IOStream* ledger = score_file_open(scores, O_RDWR | O_CREAT);
    CHECK(ledger && SDL_CloseIO(ledger));
    CHECK(save_metaruns() == 0);
    CHECK(start_new_metarun());
    CHECK(metar.id == 7003 && metarun_max == 3);
    for (int kind = 0; kind < META_STATE_DB_KIND_MAX; kind++) {
        CHECK(memory_count(kind) == 0);
        metar.id = 7001; CHECK(memory_count(kind) == 1);
        metar.id = 7002; CHECK(memory_count(kind) == 1);
        metar.id = 7003;
    }
    for (int option = OPT_meta_artefact_memory; option <= OPT_meta_legendary_places; option++)
        op_ptr->opt[option] = false;
    puts("Memory databases: user/portable paths, ownership isolation, legacy file preservation and real automatic rollover PASS.");
}

/* 0.9.8.31 has the same physical save layout as 0.9.9.0. Re-encode the
 * current record under its earlier header, rebuilding both checksums. */
static void encode_v31(byte* file, size_t size)
{
    byte previous = file[3], encoded = 31;
    u32b values = 0, bytes = 0;
    for (size_t i = 4; i < size - 8; i++) {
        byte original = file[i];
        byte value = original ^ previous;
        previous = original; encoded ^= value;
        file[i] = encoded; values += value; bytes += encoded;
    }
    file[0] = 0; file[1] = 9; file[2] = 8; file[3] = 31;
    for (int i = 0; i < 4; i++) {
        encoded ^= (byte)(values >> (i * 8));
        file[size - 8 + i] = encoded; bytes += encoded;
    }
    for (int i = 0; i < 4; i++) {
        encoded ^= (byte)(bytes >> (i * 8));
        file[size - 4 + i] = encoded;
    }
}

static void check_save_footers(void)
{
    static byte saved[2000000], legacy[2000000];
    log_set_quiet(true); /* Corrupt fixtures deliberately exercise errors. */
    for (int dead = 0; dead <= 1; dead++) {
        reset_map(5); p_ptr->is_dead = dead != 0;
        turn = playerturn = 123; Rand_state_init(12345);
        size_t size = fixture_write_full(saved, sizeof(saved));
        CHECK(size > 8);
        memcpy(legacy, saved, size); encode_v31(legacy, size);
        for (int old = 0; old <= 1; old++) {
            byte* file = old ? legacy : saved;
            CHECK(fixture_read_full(file, size) == 0);
            for (int missing = 1; missing <= 8; missing++)
                CHECK(fixture_read_full(file, size - missing) != 0);
            CHECK(fixture_read_full(file, size / 2) != 0);
            file[size] = 0;
            CHECK(fixture_read_full(file, size + 1) != 0);
            file[size - 1] ^= 1;
            CHECK(fixture_read_full(file, size) != 0);
            file[size - 1] ^= 1;
            CHECK(fixture_read_full(file, size) == 0);
        }
    }
    log_set_quiet(false);
    puts("Complete current/v31 saves: alive/dead roundtrips, all eight footer truncations, payload truncation, trailing bytes, bad checksum and next-load recovery PASS.");
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--portable", action="store_true")
    args = parser.parse_args()
    build = ROOT / ("build-portable" if args.portable else "build-standard")
    out = ROOT / "scripts/output/persistence-review-fixes" / build.name
    out.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    harness = prefix + fixture_function("terminal_extra") + "\n" + fixture_function("reset_map")
    harness += "\n" + TESTS + "\n" + init + r'''
    char install[1024], meta[1024], tales[1024], saves[1024];
    CHECK(path_build(install, sizeof(install), argv[2], "install-apex"));
    CHECK(path_build(meta, sizeof(meta), argv[2], "profile-meta"));
    CHECK(path_build(tales, sizeof(tales), meta, "metaruns"));
    CHECK(path_build(saves, sizeof(saves), argv[2], "save"));
    CHECK(SDL_CreateDirectory(install)); CHECK(SDL_CreateDirectory(tales));
    CHECK(SDL_CreateDirectory(saves));
    ANGBAND_DIR_APEX = install; ANGBAND_DIR_METARUN = tales; ANGBAND_DIR_SAVE = saves;
    r_base = mem_alloc_array(z_info->r_max, monster_race); CHECK(r_base);
    memcpy(r_base, r_info, z_info->r_max * sizeof(*r_base));
    run_mode_set_current(RUN_MODE_STORY);
    check_save_footers(); check_memory_paths_and_rollover();
    printf("Persistence regressions: %d checks PASS.\n", checks);
    SDL_Quit(); return 0;
}
'''
    sources = []
    for name, content in (("check.c", harness), ("writer.c", WRITER), ("reader.c", READER),
                          ("memory.c", '#include "meta_state.c"\n'),
                          ("files.c", '#include "metarun/metarun-files.c"\n'),
                          ("lifecycle.c", '#include "metarun/metarun-lifecycle.c"\n')):
        path = out / name
        path.write_text(content, encoding="utf-8")
        sources.append(str(path))
    cmake = build / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/fs/save.c.obj", "/src/fs/load.c.obj",
                "/src/meta_state.c.obj", "/src/metarun/metarun-lifecycle.c.obj",
                "/src/metarun/metarun-files.c.obj")
    response = out / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects if not obj.endswith(excluded)))
    flags = (cmake / "flags.make").read_text()
    defines = shlex.split(re.search(r"^C_DEFINES = (.*)$", flags, re.M).group(1))
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(build / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env.get("PATH", "")])
    exe = out / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", *defines, "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", *sources,
                    "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-o", str(exe)],
                   cwd=build, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="data-", dir=out) as data:
        result = subprocess.run([str(exe), str(ROOT / "lib/edit"), data], cwd=data,
                                env=env, capture_output=True, text=True, timeout=120)
        (out / "validation.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        print(result.stdout, end="")
        if result.returncode:
            print(result.stderr[-6000:])
            result.check_returncode()


if __name__ == "__main__":
    main()

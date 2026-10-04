#!/usr/bin/env python3
"""Check score-ledger creation, template caches and complete savefile checksum footers.

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
static bool fail_save_flush;
static int save_flush_calls;
bool __real_SDL_FlushIO(SDL_IOStream* stream);
bool __wrap_SDL_FlushIO(SDL_IOStream* stream)
{
    ++save_flush_calls;
    if (fail_save_flush)
        return SDL_SetError("Simulated save flush failure");
    return __real_SDL_FlushIO(stream);
}
bool fixture_write_flush(bool fail)
{
    fff = SDL_IOFromDynamicMem(); assert(fff);
    fail_save_flush = fail;
    save_flush_calls = 0;
    bool ok = wr_savefile();
    assert(save_flush_calls == 1);
    fail_save_flush = false;
    assert(SDL_CloseIO(fff)); fff = NULL;
    return ok;
}
bool fixture_save_flush(bool fail)
{
    fail_save_flush = fail;
    save_flush_calls = 0;
    bool ok = save_player();
    assert(save_flush_calls == 1);
    fail_save_flush = false;
    fff = NULL;
    return ok;
}
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
void fixture_restore_monster_races(void) { restore_monster_races_from_base(); }
'''

TESTS = r'''
#include "metarun.h"
#include "score/score_paths.h"
#include "score/score_io.h"
#include "blitz.h"
#include "fs/path.h"
#include "fs/io_sdl.h"

size_t fixture_write_full(byte*, size_t);
bool fixture_write_flush(bool);
bool fixture_save_flush(bool);
int fixture_read_full(const byte*, size_t);
void reset_defaults(metarun*);
bool start_new_metarun(void);
extern metarun* metaruns;
extern s16b metarun_max, current_run;
static int checks;
#define CHECK(t) do { checks++; if (!(t)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #t); exit(1); } } while (0)

static void check_ledgers(void)
{
    char scores[1024], malformed[1024], preserved[3];
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
    metarun_max = 1; current_run = 0;
    metaruns = mem_alloc_array(1, metarun); CHECK(metaruns);
    reset_defaults(&metaruns[0]); metaruns[0].id = 7001; metar = metaruns[0];
    CHECK(score_build_meta_path(scores, sizeof(scores), "scores.raw"));
    /* The new Tale exercises create_empty_scorefile through the actual
     * rollover transaction and its incoming ledger activation. */
    SDL_IOStream* initial = score_file_open(scores, O_RDWR | O_CREAT);
    CHECK(initial && SDL_CloseIO(initial));
    CHECK(save_metaruns() == 0);
    CHECK(start_new_metarun());
    CHECK(metar.id == 7002 && metarun_max == 2);
    SDL_IOStream* ledger = score_file_open(scores, O_RDWR); CHECK(ledger);
    CHECK(SDL_GetIOSize(ledger) == (Sint64)sizeof(score_file_header));
    CHECK(SDL_CloseIO(ledger));
    puts("Score ledgers: malformed file preservation and real automatic rollover PASS.");
}
void fixture_check_raw_cache(void);
void fixture_restore_monster_races(void);
static void check_revenge_caps(void)
{
    int ordinary=0, unique=0;
    for(int i=1;i<z_info->r_max;i++) {
        if(r_base[i].flags1 & RF1_UNIQUE) { if(!unique) unique=i; }
        else if(!ordinary) ordinary=i;
    }
    CHECK(ordinary && unique);
    r_info[ordinary].max_num=1; r_info[unique].max_num=1;
    fixture_restore_monster_races();
    CHECK(r_info[ordinary].max_num==100 && r_info[unique].max_num==1);
    r_info[ordinary].max_num=r_info[unique].max_num=0;
    fixture_restore_monster_races();
    CHECK(r_info[ordinary].max_num==0 && r_info[unique].max_num==0);
    r_info[ordinary]=r_base[ordinary]; r_info[unique]=r_base[unique];
    puts("Monster race restoration: temporary revenge cap reset, real uniques and slain foes preserved PASS.");
}

static void check_save_footers(void)
{
    static byte saved[2000000];
    log_set_quiet(true); /* Corrupt fixtures deliberately exercise errors. */
    reset_map(5);
    CHECK(fixture_write_flush(false));
    CHECK(!fixture_write_flush(true));
    {
        char previous_path[1024];
        size_t before_size, after_size;
        SDL_strlcpy(previous_path, savefile, sizeof(previous_path));
        SDL_strlcpy(savefile, "flush-preserved.sav", sizeof(savefile));
        CHECK(fixture_save_flush(false));
        void* before = SDL_LoadFile(savefile, &before_size);
        CHECK(before && before_size > 8);
        CHECK(!fixture_save_flush(true));
        void* after = SDL_LoadFile(savefile, &after_size);
        CHECK(after && after_size == before_size);
        CHECK(!memcmp(before, after, before_size));
        CHECK(!SDL_IOFromFile("flush-preserved.sav.new", "rb"));
        CHECK(!SDL_IOFromFile("flush-preserved.sav.old", "rb"));
        SDL_free(before); SDL_free(after);
        SDL_strlcpy(savefile, previous_path, sizeof(savefile));
    }
    puts("Save stream: successful SDL3 flush accepted; failed flush aborts and preserves the previous save byte-for-byte PASS.");
    for (int dead = 0; dead <= 1; dead++) {
        reset_map(5); p_ptr->is_dead = dead != 0;
        turn = playerturn = 123; Rand_state_init(12345);
        size_t size = fixture_write_full(saved, sizeof(saved));
        CHECK(size > 8);
        {
            byte* file = saved;
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
    puts("Complete current saves: alive/dead roundtrips, all eight footer truncations, payload truncation, trailing bytes, bad checksum and next-load recovery PASS.");
}
'''


RAW_TESTS = r'''
#include "init/init-info.c"
#include <assert.h>
static errr raw_fixture_parser(char* buf, header* head) { (void)buf; (void)head; return 0; }
void fixture_check_raw_cache(void)
{
    header expected={0}, disk={0};
    init_header(&expected, 2, sizeof(u32b)); expected.parse_info_txt=raw_fixture_parser;
    disk=expected; disk.name_size=4; disk.text_size=4;
    disk.info_ptr=(void*)1; disk.name_ptr=(void*)2; disk.text_ptr=(void*)3;
    disk.parse_info_txt=NULL;
    byte bytes[sizeof(header)+16]; memcpy(bytes,&disk,sizeof(disk));
    memset(bytes+sizeof(disk),0,16);
    for(size_t length=sizeof(header);length<sizeof(bytes);length++) {
        header actual=expected;
        SDL_IOStream* stream=SDL_IOFromConstMem(bytes,length); assert(stream);
        assert(init_info_raw(stream,&actual)!=0);
        assert(!memcmp(&actual,&expected,sizeof(actual))); SDL_CloseIO(stream);
    }
    header actual=expected;
    SDL_IOStream* stream=SDL_IOFromConstMem(bytes,sizeof(bytes)); assert(stream);
    assert(init_info_raw(stream,&actual)==0);
    assert(actual.parse_info_txt==raw_fixture_parser);
    assert(actual.info_ptr!=(void*)1 && actual.name_ptr!=(void*)2 && actual.text_ptr!=(void*)3);
    assert(actual.name_size==4 && actual.text_size==4); SDL_CloseIO(stream);
    mem_free(actual.info_ptr); mem_free(actual.name_ptr); mem_free(actual.text_ptr);
    puts("Template cache: truncated payloads rejected without publishing state, disk pointers ignored PASS.");
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
    check_save_footers(); check_ledgers(); fixture_check_raw_cache(); check_revenge_caps();
    printf("Persistence regressions: %d checks PASS.\n", checks);
    SDL_Quit(); return 0;
}
'''
    sources = []
    for name, content in (("check.c", harness), ("writer.c", WRITER), ("reader.c", READER),
                          ("raw.c", RAW_TESTS),
                          ("files.c", '#include "metarun/metarun-files.c"\n'),
                          ("lifecycle.c", '#include "metarun/metarun-lifecycle.c"\n')):
        path = out / name
        path.write_text(content, encoding="utf-8")
        sources.append(str(path))
    cmake = build / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/fs/save.c.obj", "/src/fs/load.c.obj",
                "/src/init/init-info.c.obj", "/src/metarun/metarun-lifecycle.c.obj",
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
                    "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
                    "-Wl,--wrap=SDL_FlushIO", "-o", str(exe)],
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

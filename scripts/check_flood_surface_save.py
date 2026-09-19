#!/usr/bin/env python3
"""Exercise production flood marker serialization and checksum-preserving IO.

Uses temporary streams only; covers legacy v15 and version-gated v16.
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "scripts/output/flood-surface-save"


def function(path, name):
    source = path.read_text(encoding="utf-8-sig")
    match = re.search(r"^(?:static )?(?:void|bool|byte|errr)\s+" + name + r"\(", source, re.M)
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


STUBS = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t byte;
typedef uint16_t u16b;
typedef uint32_t u32b;
typedef int64_t Sint64;
typedef int errr;
typedef const char* cptr;
#include "cave/cave-flood.h"
static FILE* fff;
static byte xor_byte;
static u32b v_check, x_check, v_stamp, x_stamp, load_byte_offset, save_byte_offset;
static bool write_error;
static int version;
static struct { int cur_map_hgt, cur_map_wid; } player = {32,32};
#define p_ptr (&player)
#define FEAT_WATER 1
#define FEAT_DEEP_WATER 2
#define FEAT_POISON 3
static byte terrain[32][32];
static byte (*cave_feat)[32] = terrain;
static bool flood_surface[32][32];
static byte flood_surface_kind[32][32];
static bool in_bounds(int y,int x) { return y>=0&&x>=0&&y<32&&x<32; }
static bool in_bounds_fully(int y,int x) { return y>0&&x>0&&y<31&&x<31; }
static bool savefile_version_at_least(byte a,byte b,byte c,byte d) {
    assert(a==0&&b==9&&c==8); return version>=d;
}
static void fake_log(cptr text, ...) { (void)text; }
static void note(cptr text) { (void)text; }
#define log_error fake_log
#define log_debug fake_log
#define log_trace fake_log
#define SDL_ReadIO(stream, buf, count) fread(buf, 1, count, stream)
#define SDL_WriteIO(stream, buf, count) fwrite(buf, 1, count, stream)
#define SDL_GetError() "test stream error"
#define SDL_TellIO ftell
static Sint64 SDL_GetIOSize(FILE* stream) {
    long pos=ftell(stream); assert(!fseek(stream,0,SEEK_END));
    long size=ftell(stream); assert(!fseek(stream,pos,SEEK_SET)); return size;
}
'''

TESTS = r'''
static byte saved[4096];
static size_t length;
static const char* stream_path;
static void begin_write(void) {
    fff=fopen(stream_path,"w+b"); assert(fff);
    xor_byte=0; v_stamp=x_stamp=save_byte_offset=0; write_error=false;
}
static void end_write(void) {
    wr_u32b(v_stamp); wr_u32b(x_stamp);
    length=(size_t)ftell(fff); assert(!write_error&&length<sizeof(saved));
    rewind(fff); assert(fread(saved,1,length,fff)==length); fclose(fff);
}
static bool read_saved(size_t size,int ver) {
    fff=fopen(stream_path,"w+b"); assert(fff);
    assert(fwrite(saved,1,size,fff)==size); rewind(fff);
    xor_byte=0; v_check=x_check=load_byte_offset=0; version=ver;
    bool ok=rd_flood_surface_markers()==0;
    if(ok) {
        u32b expected=v_check, actual=0; rd_u32b(&actual);
        ok=actual==expected;
        expected=x_check; rd_u32b(&actual);
        ok=ok&&actual==expected&&load_byte_offset==size;
    }
    fclose(fff); return ok;
}
static void malformed(int y,int x,int kind,int count) {
    begin_write(); wr_u16b(CAVE_FLOOD_SURFACE_SAVE_MAGIC); wr_u16b(count);
    for(int i=0;i<count&&i<2;i++) {wr_byte(y);wr_byte(x);wr_byte(kind);}
    end_write(); assert(!read_saved(length,15)); assert(!read_saved(length,16));
}
int main(int argc,char**argv) {
    assert(argc==2); stream_path=argv[1];
    for(int y=1;y<31;y++)for(int x=1;x<31;x++)terrain[y][x]=FEAT_WATER;
    terrain[20][20]=FEAT_POISON;
    /* Legacy files retain their reconstructed surface and consume no block. */
    cave_flood_clear_surface_markers(); assert(cave_flood_restore_surface(2,2,1));
    begin_write(); end_write();
    assert(read_saved(length,14)&&cave_flood_surface_at(2,2));
    assert(read_saved(length,15)&&cave_flood_surface_at(2,2));
    assert(!read_saved(length,16));
    /* Only v16 restores the precise narrow flood plus acid. A block carrying
     * the old version is rejected instead of inferred from its contents. */
    cave_flood_clear_surface_markers();
    for(int x=2;x<27;x++)assert(cave_flood_restore_surface(10,x,1));
    assert(cave_flood_restore_surface(20,20,2));
    begin_write(); wr_flood_surface_markers(); end_write();
    assert(!read_saved(length,15));
    {
        cave_flood_clear_surface_markers(); assert(cave_flood_restore_surface(2,2,1));
        assert(read_saved(length,16)); assert(!cave_flood_surface_at(2,2));
        for(int x=2;x<27;x++)assert(cave_flood_surface_kind_at(10,x)==1);
        assert(cave_flood_surface_kind_at(20,20)==2);
    }
    for(size_t cut=0;cut<length;cut++)assert(!read_saved(cut,16));
    saved[length-1]^=1; assert(!read_saved(length,16)); saved[length-1]^=1;
    /* Empty exact set must clear the legacy approximation. */
    cave_flood_clear_surface_markers(); begin_write(); wr_flood_surface_markers(); end_write();
    assert(cave_flood_restore_surface(2,2,1));
    assert(!read_saved(length,15)&&cave_flood_surface_at(2,2));
    assert(read_saved(length,16)&&!cave_flood_surface_at(2,2));
    malformed(0,2,1,1); malformed(32,2,1,1); malformed(2,2,0,1);
    malformed(2,2,3,1); malformed(2,2,2,1); malformed(2,2,1,2);
    malformed(2,2,1,901);
    begin_write(); wr_u16b(0xF103); wr_u16b(0); end_write(); assert(!read_saved(length,16));
    puts("Flood surface save: legacy v14/v15, version-gated v16, rejected mislabeled v15, exact/empty sets, truncation, corruption, bounds, kinds, duplicates and both checksums PASS");
    return 0;
}
'''


def main():
    source = STUBS
    for path, names in (
        ("fs/load.c", ("load_only_checksums_remain", "sf_get", "rd_byte", "rd_u16b", "rd_u32b")),
        ("fs/save.c", ("sf_put", "wr_byte", "wr_u16b", "wr_u32b")),
        ("cave/cave-flood.c", ("flood_kind_for_feature", "flood_surface_feature",
                              "cave_flood_clear_surface_markers", "cave_flood_surface_at",
                              "cave_flood_surface_kind_at", "cave_flood_restore_surface")),
        ("fs/save-dungeon.c", ("wr_flood_surface_markers",)),
        ("fs/load-dungeon.c", ("rd_flood_surface_markers",)),
    ):
        for name in names:
            source += "\n" + function(ROOT / "src" / path, name)
    OUT.mkdir(parents=True, exist_ok=True)
    fixture = OUT / "check.c"
    fixture.write_text(source + "\n" + TESTS, encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env.get("PATH", "")])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-std=c17", "-Wall", "-Wextra", "-Werror",
                    "-O0", "-I" + str(ROOT / "src"), str(fixture), "-o", str(exe)], env=env, check=True)
    with tempfile.TemporaryDirectory(dir=OUT) as directory:
        subprocess.run([str(exe), str(Path(directory) / "save.bin")], env=env, check=True, timeout=15)


if __name__ == "__main__":
    main()

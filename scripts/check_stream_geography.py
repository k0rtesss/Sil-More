"""Compile and exercise the production pure world-geography functions.

No Python reimplementation: the fixture extracts the marked, engine-independent
C section, retaining the real feature constants. Temporary files are discarded.
"""
from pathlib import Path
import argparse
import os
import re
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default=None)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    compiler = args.cc or shutil.which("cc") or shutil.which("gcc")
    if not compiler and Path("C:/msys64/mingw64/bin/gcc.exe").exists():
        compiler = "C:/msys64/mingw64/bin/gcc.exe"
    if not compiler:
        parser.error("A C17 compiler is required (--cc).")
    source = (root / "src/level-generation/level-stream-generation.c").read_text()
    pure = source[source.index("static u64b stream_mix"):source.index("/* PURE_GEOGRAPHY_END */")]
    definitions = (root / "src/defines.h").read_text()
    constants = "\n".join(re.findall(
        r"^#define FEAT_(?:WATER|DEEP_WATER|CHASM|LAVA|POISON)\s+\S+", definitions, re.M))
    harness = r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
typedef uint64_t u64b;
typedef int64_t s64b;
enum { CAVE_WATER_FLOW_CALM, CAVE_WATER_FLOW_NORTH, CAVE_WATER_FLOW_EAST,
    CAVE_WATER_FLOW_SOUTH, CAVE_WATER_FLOW_WEST };
'''
    test = r'''
static int family(int f) { return f == FEAT_DEEP_WATER ? FEAT_WATER : f; }
int main(void)
{
    const s64b extremes[] = {INT64_MIN, INT64_MIN+1, -257, -256, -255,
        -129, -128, -127, -45, -44, -43, -1, 0, 1, 43, 44, 45,
        127, 128, 129, 255, 256, 257, INT64_MAX-1, INT64_MAX};
    for (unsigned i=0; i<sizeof(extremes)/sizeof(*extremes); ++i)
        for (int divisor=1; divisor<=256; ++divisor)
        {
            s64b v=extremes[i], q=level_stream_floor_div(v, divisor);
            assert(q == v/divisor - (v%divisor<0));
            int f,d;
            level_stream_major_feature(987, 20, v, v, &f, &d);
        }
    unsigned long long wet=0, currents=0, lakes=0, materials=0;
    u64b checksum=0;
    for (int seed=0; seed<12; ++seed)
        for (int depth=1; depth<=25; depth+=6)
            for (int y=-320; y<320; ++y)
                for (int x=-320; x<320; ++x)
                {
                    int f,d,f2,d2;
                    bool found=level_stream_major_feature(seed,depth,y,x,&f,&d);
                    bool again=level_stream_major_feature(seed,depth,y,x,&f2,&d2);
                    assert(found==again && f==f2 && d==d2);
                    if (!found) continue;
                    wet++;
                    checksum ^= level_stream_hash(seed,depth,y,x,991) ^ f ^ d;
                    materials |= f==FEAT_WATER ? 1 : f==FEAT_POISON ? 2
                        : f==FEAT_LAVA ? 4 : f==FEAT_CHASM ? 8 : 16;
                    if (depth<7) assert(f!=FEAT_CHASM);
                    if (depth<10) assert(f!=FEAT_POISON);
                    if (depth<14) assert(f!=FEAT_LAVA);
                    if (f==FEAT_DEEP_WATER) { lakes++; assert(d==0); }
                    if (d)
                    {
                        int ny=y+(d==1?-1:d==3?1:0);
                        int nx=x+(d==4?-1:d==2?1:0);
                        assert(level_stream_major_feature(seed,depth,ny,nx,&f2,&d2));
                        assert(family(f)==family(f2));
                        assert(!(d==1 && d2==3) && !(d==3 && d2==1));
                        assert(!(d==2 && d2==4) && !(d==4 && d2==2));
                        currents++;
                    }
                }
    assert(wet>10000 && currents>10000 && lakes>100 && materials==31);
    /* A channel crosses every sampled longitudinal line: no disconnected
     * fragments at macro nodes, at zero, or at any 44-tile window seam. */
    for (int seed=0; seed<6; ++seed)
    {
        bool v=(level_stream_hash(seed,20,0,0,100)&1)!=0;
        for (int along=-1024; along<1024; ++along)
        {
            int count=0;
            for (int across=0; across<256; ++across)
            {
                int f,d;
                count+=level_stream_major_feature(seed,20,v?along:across,
                    v?across:along,&f,&d);
            }
            assert(count>=3);
        }
    }
    printf("stream geography passed: wet=%llu currents=%llu lake=%llu checksum=%llu\n",
        wet,currents,lakes,(unsigned long long)checksum);
    return 0;
}
'''
    env = os.environ.copy()
    env["PATH"] = str(Path(compiler).resolve().parent) + os.pathsep + env.get("PATH", "")
    with tempfile.TemporaryDirectory(prefix="sil-stream-geography-") as temp:
        path = Path(temp)
        c_file = path / "check.c"
        binary = path / ("check.exe" if os.name == "nt" else "check")
        c_file.write_text(harness + constants + "\n" + pure + test)
        subprocess.run([compiler, "-std=c17", "-Wall", "-Wextra", "-Werror",
                        "-O2", "-UNDEBUG", str(c_file), "-o", str(binary)], check=True, env=env)
        subprocess.run([str(binary)], check=True, env=env)


if __name__ == "__main__":
    main()

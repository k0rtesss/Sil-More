#!/usr/bin/env python3
"""Compile the production quest save/load block against current and legacy streams."""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "scripts/output/quest-save"


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    save = (ROOT / "src/fs/save-player.c").read_text(encoding="utf-8-sig")
    load = (ROOT / "src/fs/load-player.c").read_text(encoding="utf-8-sig")
    save = save[save.index("    /* 0.9.8.26: explicitly versioned recovered quest state. */"):]
    save = save[:save.index("    /* Skeleton note state")]
    load = load[load.index("    memset(p_ptr->quest_followup_state, 0,"):]
    load = load[:load.index("    /* Skeleton note state")]
    harness = r'''
#include "angband.h"
#include <assert.h>
static player_type body;
player_type *p_ptr = &body;
maxima *z_info;
monster_lore *l_list;
static byte stream[256];
static int pos, extra;
static void wr_byte(byte v) { stream[pos++] = v; }
static void wr_u16b(u16b v) { wr_byte(v & 255); wr_byte(v >> 8); }
static void wr_s16b(s16b v) { wr_u16b((u16b)v); }
static void rd_byte(byte *v) { *v = stream[pos++]; }
static void rd_u16b(u16b *v) { *v = stream[pos] | (stream[pos+1] << 8); pos += 2; }
static void rd_s16b(s16b *v) { u16b n; rd_u16b(&n); *v = (s16b)n; }
static bool savefile_version_at_least(int a, int b, int c, int d)
{ (void)a; (void)b; (void)c; return extra >= d; }
static void note(cptr text) { (void)text; }
'''
    harness += "static void save_block(void) { int i;\n" + save + "}\n"
    harness += "static int load_block(void) { int i; byte marker;\n" + load + "return 0; }\n"
    harness += r'''
int main(void)
{
    for (int i=0; i<10; ++i) {
        body.quest_followup_state[i] = i % 5;
        body.quest_followup_flags[i] = i;
        body.quest_followup_depth[i] = i+1;
        body.quest_followup_progress[i] = 65535-i;
    }
    body.quest_followup_recorded = 1023;
    body.quest_lifetime_flags = 3;
    body.quest_test_sandbox = 1;
    body.quest_challenge = 5;
    body.quest_challenge_failed = body.quest_challenge_recorded = 1;
    body.orome_bow_hit_streak = 2;
    body.orome_spear_ready = 1;
    save_block();
    int size = pos;
    byte expected[256]; memcpy(expected, stream, size);
    for (extra=26; extra<=28; ++extra) {
        memset(&body, 0, sizeof(body)); pos=0;
        assert(load_block()==0 && pos==size);
        pos=0; save_block(); assert(!memcmp(stream,expected,size));
    }
    for (extra=0; extra<26; ++extra) {
        memset(&body,255,sizeof(body)); body.morgoth_hits=0; pos=0;
        assert(load_block()==0 && pos==0);
        for (int i=0;i<10;++i) {
            assert(!body.quest_followup_state[i] && !body.quest_followup_flags[i]);
            assert(!body.quest_followup_depth[i] && !body.quest_followup_progress[i]);
        }
        assert(!body.quest_followup_recorded && !body.quest_test_sandbox);
        assert(!body.quest_challenge && !body.quest_lifetime_flags);
        assert(!body.orome_spear_ready && !body.orome_bow_hit_streak);
    }
    extra=26;
    stream[0]=0; pos=0; assert(load_block()==-1);
    memcpy(stream,expected,size); stream[1]=5; pos=0; assert(load_block()==-1);
    memcpy(stream,expected,size); stream[2]=16; pos=0; assert(load_block()==-1);
    memcpy(stream,expected,size); stream[3]=255;stream[4]=255;pos=0;assert(load_block()==-1);
    memcpy(stream,expected,size); stream[size-1]=2;pos=0;assert(load_block()==-1);
    printf("Quest save: %d-byte roundtrip v26/v27/v28, 26 legacy versions, corrupt marker/state/depth/flags: PASS\n",size);
    return 0;
}
'''
    source = OUT / "check.c"
    source.write_text(harness, encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env.get("PATH", "")])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-std=c17", "-O0",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "-o", str(exe)],
                   cwd=ROOT / "build-standard", env=env, check=True)
    subprocess.run([str(exe)], env=env, check=True, timeout=15)


if __name__ == "__main__":
    main()

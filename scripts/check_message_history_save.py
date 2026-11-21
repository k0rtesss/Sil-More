#!/usr/bin/env python3
"""Round-trip message records through the production save/load codecs.

Works entirely in memory; it does not open a player save or profile.
"""
from pathlib import Path
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/message-history-save"

WRITER = r'''
#include "fs/save.c"
#include <assert.h>
size_t fixture_write_messages(byte *buffer,size_t capacity,const char **messages,
    int count,bool sequences)
{
    fff=SDL_IOFromMem(buffer,capacity); assert(fff);
    xor_byte=0; v_stamp=x_stamp=save_byte_offset=0; write_error=false;
    wr_s16b(count);
    for(int i=0;i<count;i++) {
        wr_string(messages[i]); wr_u16b((u16b)(i+1));
        if(sequences) wr_u32b((u32b)(1000+i));
    }
    wr_u32b(0xA1B2C3D4U);
    size_t n=(size_t)SDL_TellIO(fff); assert(!write_error);
    SDL_CloseIO(fff); fff=NULL; return n;
}
'''

READER = r'''
#include "fs/load.c"
#include <assert.h>
void fixture_read_messages(const byte *buffer,size_t length,bool sequences)
{
    fff=SDL_IOFromConstMem(buffer,length); assert(fff);
    xor_byte=0; v_check=x_check=load_byte_offset=0;
    savefile_has_combat_history=sequences;
    rd_messages();
    u32b sentinel=0; rd_u32b(&sentinel);
    assert(sentinel==0xA1B2C3D4U && SDL_TellIO(fff)==(Sint64)length);
    SDL_CloseIO(fff); fff=NULL;
}
'''

CHECK = r'''
#include "angband.h"
#include "support/message.h"
#include "log/log.h"
#include <assert.h>
size_t fixture_write_messages(byte *,size_t,const char **,int,bool);
void fixture_read_messages(const byte *,size_t,bool);
int main(void)
{
    log_set_quiet(true); assert(SDL_Init(SDL_INIT_EVENTS));
    static const char achievement[]="Character achievement available: The Oath He Should Have Sworn. "
        "As Fëanor, escape with a Silmaril and an unbroken Oath of Mercy.";
    static char boundary126[127], boundary127[128], boundary128[129];
    static char long_utf8[1024], max_accepted[MESSAGE_BUF/4];
    static byte data[MESSAGE_BUF];
    memset(boundary126,'a',sizeof(boundary126)-1);
    memset(boundary127,'b',sizeof(boundary127)-1);
    memset(boundary128,'c',sizeof(boundary128)-1);
    memset(long_utf8,'d',sizeof(long_utf8)-1);
    /* Multibyte characters straddle the historical 127-byte limit. */
    memcpy(long_utf8+126,"ë",2);
    memcpy(long_utf8+sizeof(long_utf8)-8,"finish!",7);
    memset(max_accepted,'e',sizeof(max_accepted)-1);
    memcpy(max_accepted+sizeof(max_accepted)-8,"finish!",7);
    const char *messages[]={boundary126,boundary127,boundary128,
        achievement,long_utf8,max_accepted};
    assert(strlen(achievement)>127);
    for(int sequences=0;sequences<2;sequences++) {
        assert(messages_init()==0);
        size_t n=fixture_write_messages(data,sizeof(data),messages,
            N_ELEMENTS(messages),sequences);
        fixture_read_messages(data,n,sequences);
        assert(message_num()==N_ELEMENTS(messages));
        for(int i=0;i<N_ELEMENTS(messages);i++) {
            int age=N_ELEMENTS(messages)-1-i;
            assert(!strcmp(message_str(age),messages[i]));
            assert(message_type(age)==i+1);
            if(sequences) assert(message_sequence(age)==(u32b)(1000+i));
        }
        messages_free();
    }
    puts("Message history save/load: 126/127/128-byte boundaries, actual UTF-8 achievement, 1023 bytes, ring maximum, old/new sequence records and stream alignment PASS");
    SDL_Quit(); return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    sources = []
    for name, content in (("writer.c", WRITER), ("reader.c", READER),
                          ("check.c", CHECK)):
        path = OUT / name
        path.write_text(content, encoding="utf-8")
        sources.append(str(path))
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/fs/save.c.obj", "/src/fs/load.c.obj")
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + o + '"' for o in objects if not o.endswith(excluded)),
                        encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / d) for d in
        ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", *sources, "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-o", str(exe)],
        cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe)], cwd=OUT, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()

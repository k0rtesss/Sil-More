#!/usr/bin/env python3
"""Exercise production UTF-8 clipping, string bounds, gap buffers and macros.

Run after configuring a Windows build. Recompiles the support units under test;
fixtures use guarded buffers and never open user files.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "scripts/output/support-bounds"

HARNESS = r'''
#include "angband.h"
#include "externs.h"
#include "support/utf8.h"
#include "support/editing-buffer.h"
#include "support/macro.h"
#include <assert.h>
#include <string.h>

static void check_utf8(void)
{
    const char text[] = "ab\xc3\xa9\xe4\xb8\xad\xf0\x9f\x92\x8e" "x";
    const int expected[] = {0,1,2,2,4,4,4,7,7,7,7,11,12};
    for (int n=0; n<(int)N_ELEMENTS(expected); n++)
        assert(utf8_safe_prefix_len(text,n)==expected[n]);
    assert(utf8_safe_prefix_len(text,100)==12);
    assert(utf8_safe_prefix_len(NULL,4)==0);
    assert(utf8_safe_prefix_len("\xff" "x",2)==2);
    const char bounded[] = {'a',(char)0xe4,(char)0xb8};
    assert(utf8_safe_prefix_len(bounded,sizeof(bounded))==1);
    assert(utf8_sequence_len_n(bounded+1,2)==1);
    assert(utf8_display_width_n(text,sizeof(text)-1)==6);
    puts("UTF-8: every clipping boundary, bounded unterminated input and display widths PASS.");
}

static void check_strl(void)
{
    char zero[] = "Z";
    assert(SDL_strlcat(zero,"tail",0)==4 && !strcmp(zero,"Z"));
    assert(SDL_strlcat(NULL,"tail",0)==4);
    char full[] = "abcd";
    assert(SDL_strlcat(full,"xy",3)==5 && !strcmp(full,"abcd"));
    char bounded[] = {'a','b','c','Z'};
    assert(SDL_strlcat(bounded,"xy",3)==5);
    assert(!memcmp(bounded,"abcZ",4));
    struct { char text[5]; char guard[8]; } out;
    memset(&out,'Z',sizeof(out)); out.text[0]='a'; out.text[1]=0;
    assert(SDL_strlcat(out.text,"bcdef",sizeof(out.text))==6);
    assert(!strcmp(out.text,"abcd"));
    assert(!memcmp(out.guard,"ZZZZZZZZ",8));
    assert(SDL_strlcpy(out.text,"abcdef",sizeof(out.text))==6);
    assert(!strcmp(out.text,"abcd"));
    puts("String copies/appends: zero capacity, unterminated destination, truncation and canaries PASS.");
}

static void check_editing(void)
{
    editing_buffer eb = {0};
    char text[80];
    editing_buffer_init(&eb,"abcdefghijklmnop",40);
    editing_buffer_get_all(&eb,text,sizeof(text));
    assert(!strcmp(text,"abcdefghijklmnop"));
    assert(editing_buffer_set_position(&eb,3));
    assert(editing_buffer_put_str(&eb,"XY",-1)==2);
    assert(editing_buffer_delete(&eb));
    editing_buffer_get_all(&eb,text,sizeof(text));
    assert(!strcmp(text,"abcXYefghijklmnop"));
    editing_buffer_clear(&eb);
    assert(editing_buffer_put_str(&eb,"new",-1)==3);
    editing_buffer_get_all(&eb,text,sizeof(text)); assert(!strcmp(text,"new"));
    editing_buffer_destroy(&eb);
    assert(!editing_buffer_put_chr(&eb,'x'));
    assert(!editing_buffer_put_str(&eb,"x",-1));
    editing_buffer_init(&eb,"too long",4);
    editing_buffer_get_all(&eb,text,sizeof(text));
    assert(!strcmp(text,"too") && EDITING_BUFFER_LEN(&eb)==3);
    assert(!editing_buffer_put_chr(&eb,'x'));
    text[0]='Z'; editing_buffer_get_all(&eb,text,0); assert(text[0]=='Z');
    editing_buffer_get_all(NULL,text,sizeof(text)); assert(text[0]==0);
    editing_buffer_destroy(&eb);
    editing_buffer_init(&eb,"ignored",0);
    assert(!eb.buf && eb.pos==0 && eb.max_size==0 && eb.gap_size==0);
    assert(!editing_buffer_put_chr(&eb,'x'));
    editing_buffer_destroy(&eb);
    editing_buffer_init(&eb,"ignored",1);
    editing_buffer_get_all(&eb,text,sizeof(text)); assert(text[0]==0);
    editing_buffer_destroy(&eb);
    puts("Gap buffer: full initial text, truncated initialization, cursor editing, zero capacity and destroyed state PASS.");
}

static void check_macros(void)
{
    char output[128];
    macro_template = "&#";
    macro_modifier_chr = "S";
    macro_modifier_name[0] = "Shift-";
    max_macrotrigger = 1;
    macro_trigger_name[0] = "F1";
    macro_trigger_keycode[0][0] = "11";
    macro_trigger_keycode[1][0] = "12";
    const char ascii[] = "ab\x1f" "S12\rcd\x1f" "11\rz";
    text_to_ascii(output,sizeof(output),"ab\\[Shift-F1]cd\\[F1]z");
    assert(!strcmp(output,ascii));
    ascii_to_text(output,sizeof(output),ascii);
    assert(!strcmp(output,"ab\\[Shift-F1]cd\\[F1]z"));
    text_to_ascii(output,sizeof(output),"\\qend"); assert(!strcmp(output,"qend"));
    char guarded[9];
    for (size_t size=0; size<sizeof(guarded)-1; size++) {
        memset(guarded,'Z',sizeof(guarded));
        text_to_ascii(guarded,size,"ab\\[Shift-F1]cd");
        assert(guarded[size]=='Z');
        if (size) assert(memchr(guarded,0,size));
        memset(guarded,'Z',sizeof(guarded));
        ascii_to_text(guarded,size,ascii);
        assert(guarded[size]=='Z');
        if (size) assert(memchr(guarded,0,size));
    }
    ascii_to_text(output,sizeof(output),"a\x1f"); assert(!strcmp(output,"a^_"));
    ascii_to_text(output,sizeof(output),"a\x1f" "S"); assert(!strcmp(output,"a^_S"));
    ascii_to_text(output,sizeof(output),"a\x1f" "garbage\r!");
    assert(!strcmp(output,"a^_garbage\\r!"));
    macro_template=NULL; macro_modifier_chr=NULL; max_macrotrigger=0;
    puts("Macros: prefixed/repeated shifted triggers, malformed escapes, small buffers and roundtrips PASS.");
}

static int quantity_navigation, quantity_input_step;
static errr quantity_input_hook(int action, int value)
{
    (void)value;
    if (action != TERM_XTRA_EVENT) return 0;
    if (quantity_input_step++ == 0)
        assert(sdl_question_menu_queue_navigation(quantity_navigation));
    else
        assert(!Term_keypress('\r'));
    return 0;
}

static void check_quantity(void)
{
    term terminal;
    assert(SDL_Init(SDL_INIT_EVENTS));
    assert(!term_init(&terminal,80,24,256));
    angband_term[0]=&terminal; Term_activate(&terminal);
    assert(!Term_keypress('9'));
    for (int n=0;n<14;n++) assert(!Term_keypress('9'));
    assert(!Term_keypress('\r'));
    p_ptr->command_arg=0;
    assert(get_quantity("How many?",99)==99);
    const char* entries[] = {"2", "8", "28", "82", "28\b", "28+", "28-"};
    const int quantities[] = {2, 8, 28, 82, 2, 29, 27};
    for (int i=0; i<(int)N_ELEMENTS(entries); i++) {
        for (const char* key=entries[i]; *key; key++)
            assert(!Term_keypress(*key));
        assert(!Term_keypress('\r'));
        assert(get_quantity_touch_category_force_prompt("How many?",99,0)
            == quantities[i]);
    }
    terminal.xtra_hook=quantity_input_hook;
    for (int direction=-1; direction<=1; direction+=2) {
        quantity_navigation=direction;
        quantity_input_step=0;
        assert(get_quantity_touch_category_force_prompt("How many?",99,0)
            == (direction < 0 ? 0 : 98));
        assert(quantity_input_step==2);
    }
    term_nuke(&terminal); SDL_Quit();
    puts("Quantity prompt: literal 2/8 digits, semantic arrows, backspace and +/- edits, and fifteen-digit overflow clamping PASS.");
}

int main(int argc,char** argv)
{
    assert(argc==2);
    if (!strcmp(argv[1],"utf8") || !strcmp(argv[1],"all")) check_utf8();
    if (!strcmp(argv[1],"strl") || !strcmp(argv[1],"all")) check_strl();
    if (!strcmp(argv[1],"editing") || !strcmp(argv[1],"all")) check_editing();
    if (!strcmp(argv[1],"macros") || !strcmp(argv[1],"all")) check_macros();
    if (!strcmp(argv[1],"quantity") || !strcmp(argv[1],"all")) check_quantity();
    return 0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case", choices=("all", "utf8", "strl", "editing", "macros", "quantity"), default="all")
    parser.add_argument("--portable", action="store_true")
    args = parser.parse_args()
    build = ROOT / ("build-portable" if args.portable else "build-standard")
    out = OUT / ("portable" if args.portable else "standard")
    out.mkdir(parents=True, exist_ok=True)
    source = out / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    units = ["utf8", "strl", "editing-buffer", "macro", "prompt"]
    replaced = ("/src/main.c.obj", *(f"/src/support/{unit}.c.obj" for unit in units))
    objects = shlex.split((build / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    response = out / "objects.rsp"
    response.write_text("\n".join('"'+obj+'"' for obj in objects if not obj.endswith(replaced)), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(build / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    executable = out / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g", "-Wall", "-Wextra",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
                    *(str(ROOT / "src/support" / (unit+".c")) for unit in units),
                    "@"+str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
                    "-o", str(executable)], cwd=build, env=env, check=True)
    subprocess.run([str(executable),args.case], env=env, check=True, timeout=15)


if __name__ == "__main__":
    main()

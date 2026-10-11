#!/usr/bin/env python3
"""Check complete character text exports independently of the live viewport."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build-standard'
OUT = ROOT / 'scripts/output/character-dump-layout'

HARNESS = r'''
#include "angband.h"
#include "init/init2-internal.h"
#include "ui/character-dump.h"
#include "fs/path.h"
#include "log/log.h"
#include "externs.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

const char* __wrap_get_sdl_config_path(void) { return "isolated-sdl.json"; }
static errr terminal_extra(int action, int value)
{
    (void)value;
    if (action == TERM_XTRA_EVENT) Term_keypress(' ');
    return 0;
}
static void squeeze(char* dest, const char* src)
{
    while (*src) {
        if (!isspace((unsigned char)*src)) *dest++ = *src;
        src++;
    }
    *dest = '\0';
}
int main(int argc, char** argv)
{
    term caller;
    assert(argc == 3);
    setbuf(stdout, NULL);
    log_set_quiet(true);
    assert(SDL_Init(SDL_INIT_EVENTS));
    ANGBAND_DIR_EDIT = argv[1];
    ANGBAND_DIR_DATA = ANGBAND_DIR_USER = ANGBAND_DIR_PREF = argv[2];
    ANGBAND_DIR_SAVE = ANGBAND_DIR_APEX = argv[2];
    assert(term_init(&caller, 80, 24, 32) == 0);
    caller.xtra_hook = terminal_extra;
    angband_term[0] = &caller;
    Term_activate(&caller);
    assert(init_z_info() == 0); assert(init_f_info() == 0);
    assert(init_k_info() == 0); assert(init_b_info() == 0);
    assert(init_a_info() == 0); assert(init_e_info() == 0);
    assert(init_r_info() == 0); assert(init_v_info() == 0);
    assert(init_p_info() == 0); assert(init_c_info() == 0);
    assert(init_oath_info() == 0); assert(init_cu_info() == 0);
    assert(init_mb_info() == 0); assert(init_style_info() == 0);
    assert(init_other() == 0); assert(init_alloc() == 0);
    rp_ptr = &p_info[0];
    current_character_profile = &c_info[0];
    SDL_strlcpy(op_ptr->full_name, "Export Hero", sizeof(op_ptr->full_name));
    p_ptr->chp = 44; p_ptr->mhp = 61;
    p_ptr->csp = 42; p_ptr->msp = 49;
    p_ptr->depth = 15; playerturn = 1461; turn = 14010;
    for (int stat = 0; stat < A_MAX; stat++)
        p_ptr->stat_base[stat] = p_ptr->stat_use[stat] = 5;
    for (int skill = 0; skill < S_MAX; skill++)
        p_ptr->skill_base[skill] = p_ptr->skill_use[skill] = 8;
    p_ptr->history[0] = '\0';
    for (int i = 0; i < 6; i++)
        SDL_strlcat(p_ptr->history,
            "Every word of the long biography must survive a narrow phone and a wide desktop. ",
            sizeof(p_ptr->history));
    SDL_strlcat(p_ptr->history, "EXPORT_HISTORY_END", sizeof(p_ptr->history));
    const int sizes[][2] = {{100, 22}, {50, 18}, {80, 24}, {160, 48}};
    char expected[sizeof(p_ptr->history)];
    squeeze(expected, p_ptr->history);
    for (int n = 0; n < 8; n++) {
        int width = sizes[n % 4][0], height = sizes[n % 4][1];
        bool dead = n >= 4;
        p_ptr->is_dead = dead;
        assert(Term_resize(width, height) == 0);
        Term_clear(); Term_putstr(1, 1, -1, TERM_WHITE, "LIVE_SCREEN_SENTINEL");
        Term_keypress('x');
        int key_head = caller.key_head, key_tail = caller.key_tail;
        char name[80], path[1024], text[65536], condensed[65536];
        strnfmt(name, sizeof(name), "export-%s-%dx%d.txt", dead ? "dead" : "alive", width, height);
        assert(file_character(name, false) == 0);
        path_build(path, sizeof(path), argv[2], name);
        FILE* file = fopen(path, "rb"); assert(file);
        size_t len = fread(text, 1, sizeof(text) - 1, file);
        assert(!ferror(file)); fclose(file); text[len] = '\0';
        assert(strstr(text, "Export Hero"));
        assert(strstr(text, "Str")); assert(strstr(text, "Dex"));
        assert(strstr(text, "Con")); assert(strstr(text, "Gra"));
        assert(strstr(text, "Melee")); assert(strstr(text, "Song"));
        squeeze(condensed, text);
        assert(strstr(condensed, expected));
        assert(Term == &caller);
        assert(caller.wid == width && caller.hgt == height);
        assert(caller.key_head == key_head && caller.key_tail == key_tail);
        byte attr; char cell;
        assert(Term_what(1, 1, &attr, &cell) == 0 && cell == 'L');
        assert(playerturn == 1461 && turn == 14010 && p_ptr->depth == 15);
        assert(p_ptr->chp == 44 && p_ptr->csp == 42 && p_ptr->is_dead == dead);
        assert(!dead || strstr(text, "[Last Messages]"));
        printf("Character export %s %dx%d: full header, attributes, skills and biography; caller state preserved PASS\n",
            dead ? "dead" : "alive", width, height);
    }
    Term_activate(NULL); term_nuke(&caller); SDL_Quit();
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / 'check.c'
    source.write_text(HARNESS, encoding='utf-8')
    cmake = BUILD / 'CMakeFiles/sil-more.dir'
    objects = shlex.split((cmake / 'objects1.rsp').read_text())
    response = OUT / 'objects.rsp'
    response.write_text('\n'.join('"' + name + '"' for name in objects
                                  if not name.endswith('/src/main.c.obj')))
    env = os.environ.copy()
    env['PATH'] = os.pathsep.join([
        *(str(BUILD / '_deps' / dep) for dep in ('SDL', 'SDL_ttf', 'SDL_image', 'SDL_mixer')),
        'C:/msys64/mingw64/bin', 'C:/msys64/usr/bin', env['PATH']])
    exe = OUT / 'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe', '-DUSE_SDL', '-std=c17', '-O0', '-g',
                    '@CMakeFiles/sil-more.dir/includes_C.rsp', str(source), '@' + str(response),
                    '@CMakeFiles/sil-more.dir/linkLibs.rsp', '-Wl,--wrap=get_sdl_config_path',
                    '-o', str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix='profile-', dir=OUT) as private:
        subprocess.run([str(exe), str(ROOT / 'lib/edit'), private],
                       cwd=private, env=env, check=True, timeout=45)
        for export in Path(private).glob('export-*.txt'):
            (OUT / export.name).write_bytes(export.read_bytes())


if __name__ == '__main__':
    main()

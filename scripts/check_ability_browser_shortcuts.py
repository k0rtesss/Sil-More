#!/usr/bin/env python3
"""Check the classic ability browser's real hotkeys with isolated templates."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/ability-browser-shortcuts"

CHECKS = r'''
#include "sdl-config.h"
#undef assert
#define assert(expr) do { if (!(expr)) { fprintf(stderr, "%s (%d)\n", #expr, __LINE__); exit(1); } } while (0)
static const char *input_sequence, *expected_name;
static bool saw_expected;

static errr terminal_extra(int action, int value)
{
    (void)value;
    if (action != TERM_XTRA_EVENT) return 0;
    assert(input_sequence && *input_sequence);
    if (*input_sequence == ESCAPE && expected_name) {
        char row[256], heading[160];
        strnfmt(heading, sizeof(heading), "%s:", expected_name);
        for (int y = 0; y < Term->hgt; y++) {
            assert(Term->wid < sizeof(row));
            memcpy(row, Term->scr->c[y], Term->wid);
            row[Term->wid] = '\0';
            if (strstr(row, heading)) saw_expected = true;
            assert(!strstr(row, "i) ") && !strstr(row, "q) "));
        }
    }
    Term_keypress(*input_sequence++);
    return 0;
}

static void browse(cptr keys, cptr name)
{
    player_type before = *p_ptr;
    input_sequence = keys;
    expected_name = name;
    saw_expected = false;
    Term_flush();
    do_cmd_ability_screen();
    assert(!*input_sequence && (!name || saw_expected));
    assert(p_ptr->new_exp == before.new_exp);
    assert(p_ptr->energy_use == before.energy_use);
    assert(!memcmp(p_ptr->have_ability, before.have_ability, sizeof(before.have_ability)));
    assert(!memcmp(p_ptr->active_ability, before.active_ability, sizeof(before.active_ability)));
}

static void check_shortcuts(void)
{
    sdl_config_set_defaults(&config);
    assert(Term_resize(96, 36) == 0);
    p_ptr->new_exp = 10000;
    for (int skill = 0; skill < S_MAX; skill++) p_ptr->skill_base[skill] = 20;
    character_generated = false;
    character_icky = 0;
    browse("j\033", "Warden");
    browse("s\033", "Strength");
    browse("i\033\033", "Power");
    browse("q", NULL);
    puts("Classic ability shortcuts: j/s dispatch, i Skills, q exit, reserved labels and unchanged XP/abilities PASS.");
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    setup = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    setup = setup[:setup.index("    check_templates();")]
    source = OUT / "check.c"
    source.write_text(prefix + CHECKS + setup +
                      "    check_shortcuts(); SDL_Quit(); return 0;\n}\n", encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects
                                  if not obj.endswith("/src/main.c.obj")),
                        encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / dep) for dep in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
                    "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-o", str(exe)],
                   cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="fixture-", dir=OUT) as state:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), state], cwd=state,
                       env=env, check=True, timeout=15)


if __name__ == "__main__":
    main()

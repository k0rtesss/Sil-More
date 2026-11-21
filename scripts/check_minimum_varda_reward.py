#!/usr/bin/env python3
"""Run Varda's deferred reward flow with Minimum text mode enabled."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
import check_qa9_varda_reward_defer as base

ROOT = base.ROOT
BUILD = base.BUILD
OUT = ROOT / "scripts/output/minimum-varda-reward"


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    checks = base.CHECKS.replace('static bool take_reward;', '''static bool take_reward;
static int brief_calls;
void __wrap_quest_minimum_dialogue(int quest, bool complete, cptr title,
    cptr texts[], int count, byte title_color, byte text_color)
{
    (void)title_color; (void)text_color;
    assert(quest == QUEST_ID_VARDA && complete && title && texts && count > 0);
    ++brief_calls;
}''')
    checks = checks.replace('(void)words; (void)text_count; (void)prompt; (void)labels;',
        'assert(!words && text_count == 0); (void)prompt; (void)labels;')
    checks = checks.replace('    puts("Varda:',
        '    assert(brief_calls == book_calls);\n    puts("Minimum text Varda:')
    fixture = base.HARNESS
    prefix = fixture[:fixture.index("static cptr make_path")]
    init = fixture[fixture.index("static cptr make_path"):fixture.index("    int last = ")]
    source = OUT / "check.c"
    source.write_text(prefix + checks + init
        + "    op_ptr->opt[OPT_minimum_text_mode] = true;\n"
          "    checks(); sdl_quit_hook(NULL); return 0;\n}\n", encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects if not p.endswith(
        ("/src/main.c.obj", "/src/quest/valar/tulkas-varda.c.obj"))), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / p) for p in
        ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    env.update(SDL_VIDEO_DRIVER="dummy", SDL_RENDER_DRIVER="software",
               SDL_AUDIO_DRIVER="dummy", CONTROL_TEST_ASSETS=str(ROOT / "lib"))
    exe = OUT / "check.exe"
    wraps = ("quest_reward_book_choice", "quest_minimum_dialogue", "metarun_mark_quest_completed",
             "metarun_unlock_oath", "do_cmd_note", "quest_typewriter_menu")
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
        str(ROOT / "src/quest/valar/tulkas-varda.c"), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", *("-Wl,--wrap=" + name for name in wraps),
        "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="profile-", dir=OUT) as profile:
        env["CONTROL_TEST_PROFILE"] = profile
        subprocess.run([str(exe), "--windowed", "--tiles"], cwd=ROOT, env=env,
                       check=True, timeout=30)


if __name__ == "__main__":
    main()

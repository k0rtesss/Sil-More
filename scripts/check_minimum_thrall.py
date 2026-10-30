#!/usr/bin/env python3
"""Check thrall briefs retain the actual story and preserve the random stream."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
from check_new_monsters import HARNESS
from check_monster_scent_save import fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/minimum-thrall"

CHECKS = r'''
#include "thrall-under-test.c"
static char original[2048], minimum_story[2048];
static int full_calls, brief_calls;
static bool replay_check;
void __wrap_quest_typewriter_menu_pages(cptr title, cptr texts[], int count,
    byte title_color, byte text_color, int pages)
{
    (void)title_color; (void)text_color;
    assert(strstr(title, "Thrall") && count == 1 && pages == 1);
    if (replay_check) assert(!strcmp(minimum_story, texts[0]));
    else SDL_strlcpy(original, texts[0], sizeof(original));
    ++full_calls;
}
void __wrap_quest_minimum_custom_dialogue(cptr title, cptr summary, cptr texts[],
    int count, byte title_color, byte text_color, int pages)
{
    (void)title_color; (void)text_color;
    assert(strstr(title, "Thrall") && count == 1 && pages == 1);
    assert(strstr(summary, "Objective: Give ") && strstr(summary, "Reward: 75 XP"));
    assert(strstr(summary, "choice of available services"));
    SDL_strlcpy(minimum_story, texts[0], sizeof(minimum_story));
    ++brief_calls;
}
static void check_thrall_brief(void)
{
    const int races[] = {R_IDX_ALERT_HUMAN_THRALL, R_IDX_ALERT_ELF_THRALL};
    for (int r = 0; r < 2; ++r) {
        monster_type thrall = {0};
        thrall.r_idx = races[r];
        thrall.thrall_quest_item = THRALL_QUEST_FLASK_OIL;
        Rand_state_init(0x12345678);
        op_ptr->opt[OPT_minimum_text_mode] = false;
        thrall_request_dialog(&thrall);
        u64b after_full = Rand_state_export();
        Rand_state_init(0x12345678);
        op_ptr->opt[OPT_minimum_text_mode] = true;
        thrall_request_dialog(&thrall);
        assert(Rand_state_export() == after_full);
        thrall.thrall_quest_requested = 1;
        replay_check = true;
        thrall_quest_show_request_story(&thrall);
        replay_check = false;
        assert(Rand_state_export() == after_full);
    }
    assert(full_calls == 4 && brief_calls == 2);
    /* Old saves used the same boolean; they can open a story without RNG. */
    monster_type old_thrall = {0};
    old_thrall.r_idx = R_IDX_ALERT_HUMAN_THRALL;
    old_thrall.thrall_quest_item = THRALL_QUEST_FLASK_OIL;
    old_thrall.thrall_quest_requested = 1;
    u64b before_replay = Rand_state_export();
    thrall_quest_show_request_story(&old_thrall);
    assert(Rand_state_export() == before_replay && full_calls == 5);
    op_ptr->opt[OPT_minimum_text_mode] = false;
    puts("Thrall request: brief and exact saved story, both races, legacy flag and unchanged RNG PASS");
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = HARNESS[:HARNESS.index("static const char* guids[]")]
    init = HARNESS[HARNESS.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    thrall = (ROOT / "src/thrall_quest.c").read_text(encoding="utf-8").replace('#include "externs.h"', '')
    (OUT / "thrall-under-test.c").write_text(thrall, encoding="utf-8")
    source = OUT / "check.c"
    source.write_text(prefix + fixture_function("terminal_extra") + "\n" + CHECKS + init
        + "    check_thrall_brief(); SDL_Quit(); return 0;\n}\n", encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects if not p.endswith(
        ("/src/main.c.obj", "/src/thrall_quest.c.obj"))), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / p) for p in
        ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    wraps = ("quest_typewriter_menu_pages", "quest_minimum_custom_dialogue")
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", *("-Wl,--wrap=" + name for name in wraps),
        "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="profile-", dir=OUT) as profile:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), profile], cwd=profile,
                       env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()

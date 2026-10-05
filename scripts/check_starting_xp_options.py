#!/usr/bin/env python3
"""Replay starting-budget changes through the production Options menu.

Uses the Windows engine build, isolated template/config data, and scripted input
at the terminal boundary. No player save or personal settings are opened.
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
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/starting-xp-options"

CHECKS = r'''
#include "quest/quest-challenges.h"
static const unsigned char* keys;
static int key_index;
char __wrap_inkey(void)
{
    assert(keys[key_index]);
    unsigned char key = keys[key_index++];
    if (key == 127) {
        bool wake = false;
        assert(ui_menu_click_handle_choice_action(RESET_CHOICE,
            UI_MENU_CLICK_PRIMARY, &wake));
        return 0;
    }
    return (char)key;
}
/* Persistence is outside this fixture; the production menu still owns changes. */
void __wrap_metarun_save_persistent_settings(void) {}
void __wrap_save_pane_config_to_json(void) {}

static void seed(bool fixed, int total, int available, int actions)
{
    memset(p_ptr, 0, sizeof(*p_ptr));
    op_ptr->opt[OPT_quest_challenges_beta] = false;
    birth_fixed_exp = fixed;
    p_ptr->exp = total; p_ptr->new_exp = available;
    p_ptr->energy_use = 75;
    turn = 123; playerturn = actions;
    for (int i = 0; i < S_MAX; ++i) {
        p_ptr->skill_base[i] = 10 + i;
        p_ptr->innate_ability[i][0] = true;
        p_ptr->active_ability[i][0] = true;
    }
}
static void menu(int page, const char* inputs)
{
    keys = (const unsigned char*)inputs; key_index = 0;
    int actions = playerturn;
    do_cmd_options_aux(page, "Fixture Options");
    assert(!keys[key_index]);
    assert(turn == 123 && playerturn == actions && p_ptr->energy_use == 75);
    assert(!hide_cursor);
}
static void expect_preserved(int total, int available)
{
    assert(p_ptr->exp == total && p_ptr->new_exp == available);
    for (int i = 0; i < S_MAX; ++i) {
        assert(p_ptr->skill_base[i] == 10 + i);
        assert(p_ptr->innate_ability[i][0] && p_ptr->active_ability[i][0]);
    }
}
static void expect_reset(int total)
{
    assert(p_ptr->exp == total && p_ptr->new_exp == total);
    for (int i = 0; i < S_MAX; ++i) {
        assert(!p_ptr->skill_base[i]);
        for (int j = 0; j < ABILITIES_MAX; ++j)
            assert(!p_ptr->innate_ability[i][j] && !p_ptr->active_ability[i][j]);
    }
}
static void preserve(void)
{
    seed(false, 90000, 23000, 0);
    menu(GAMEPLAY_PAGE, "2n\033"); expect_preserved(90000, 23000);
    seed(true, PY_FIXED_EXP, 17000, 0);
    menu(GAMEPLAY_PAGE, "2n\033"); expect_preserved(PY_FIXED_EXP, 17000);
    menu(CHALLENGE_PAGE, "222y\033"); expect_preserved(PY_FIXED_EXP, 17000);
    puts("Unrelated settings, menu navigation and unchanged XP mode preserve all eight skills, abilities and XP PASS.");
}
static void toggle(void)
{
    seed(false, PY_START_EXP, 1000, 0);
    menu(CHALLENGE_PAGE, "222y\033");
    assert(birth_fixed_exp); expect_reset(PY_FIXED_EXP);
    seed(true, PY_FIXED_EXP, 17000, 0);
    menu(CHALLENGE_PAGE, "222n\033");
    assert(!birth_fixed_exp); expect_reset(PY_START_EXP);
    puts("Fixed-XP mode on/off restores the correct budget and clears all eight skills and abilities PASS.");
}
static void reset(void)
{
    seed(true, PY_FIXED_EXP, 17000, 0);
    menu(CHALLENGE_PAGE, "\177\033");
    assert(!birth_fixed_exp); expect_reset(PY_START_EXP);
    puts("Mouse row-reset updates the starting budget before immediate Escape PASS.");
}
static void locked(void)
{
    seed(true, PY_FIXED_EXP, 17000, 7);
    menu(CHALLENGE_PAGE, "222n\033");
    assert(birth_fixed_exp); expect_preserved(PY_FIXED_EXP, 17000);
    puts("After the first action, challenge settings and allocation remain locked PASS.");
}
static void quest_seed(bool birth, int actions, int total, int available)
{
    seed(birth, total, available, actions);
    p_ptr->quest_challenge = CHALLENGE_FIXED_50K_XP;
    op_ptr->opt[OPT_quest_challenges_beta] = true;
    assert(quest_challenge_active(CHALLENGE_FIXED_50K_XP));
}
static void quest_toggle(bool value)
{
    char inputs[OPT_PAGE_PER + 3];
    int row = 0;
    for (int i = 0; i < OPT_PAGE_PER; ++i) {
        if (option_page[QUEST_PAGE][i] == OPT_quest_challenges_beta) break;
        if (option_page[QUEST_PAGE][i] != OPT_NONE) ++row;
    }
    assert(row < OPT_PAGE_PER);
    memset(inputs, '2', row);
    inputs[row] = value ? 'y' : 'n';
    inputs[row + 1] = ESCAPE; inputs[row + 2] = 0;
    menu(QUEST_PAGE, inputs);
    assert(op_ptr->opt[OPT_quest_challenges_beta] == value);
}
static void quest(void)
{
    quest_seed(false, 0, PY_FIXED_EXP, 17000);
    menu(CHALLENGE_PAGE, "222y\033");
    assert(birth_fixed_exp); expect_preserved(PY_FIXED_EXP, 17000);
    menu(CHALLENGE_PAGE, "222n\033");
    assert(!birth_fixed_exp); expect_preserved(PY_FIXED_EXP, 17000);
    quest_seed(true, 0, PY_FIXED_EXP, 17000);
    menu(CHALLENGE_PAGE, "\177\033");
    assert(!birth_fixed_exp); expect_preserved(PY_FIXED_EXP, 17000);
    quest_seed(false, 0, PY_FIXED_EXP, 17000);
    quest_toggle(false); expect_reset(PY_START_EXP);
    quest_seed(true, 0, PY_FIXED_EXP, 17000);
    quest_toggle(false); expect_preserved(PY_FIXED_EXP, 17000);
    seed(false, PY_START_EXP, 1000, 0);
    p_ptr->quest_challenge = CHALLENGE_FIXED_50K_XP;
    quest_toggle(true); expect_reset(PY_FIXED_EXP);
    quest_seed(false, 7, PY_FIXED_EXP, 17000);
    quest_toggle(false); expect_preserved(PY_FIXED_EXP, 17000);
    quest_seed(false, 0, 90000, 23000);
    menu(GAMEPLAY_PAGE, "\033"); expect_preserved(90000, 23000);
    puts("Quest/birth effective fixed-XP mode, row reset, enable/disable and initial grants/later-action preservation PASS.");
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", action="store_true")
    parser.add_argument("--case", choices=("all", "preserve", "toggle", "reset", "locked", "quest"), default="all")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    settings = ROOT / "src/cmd/ui/cmd-ui-settings.c"
    current = settings.read_text(encoding="utf-8")
    reset_base = int(re.search(r"#define SETTINGS_CLICK_RESET_ROW_BASE\s+(\d+)", current)[1])
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    prefix += fixture_function("terminal_extra") + "\n"
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")].replace("assert(argc==3)", "assert(argc==4)")
    calls = """    if (streq(argv[3], "all") || streq(argv[3], "preserve")) preserve();
    if (streq(argv[3], "all") || streq(argv[3], "toggle")) toggle();
    if (streq(argv[3], "all") || streq(argv[3], "reset")) reset();
    if (streq(argv[3], "all") || streq(argv[3], "locked")) locked();
    if (streq(argv[3], "all") || streq(argv[3], "quest")) quest();
    SDL_Quit(); return 0;
}
"""
    harness = OUT / "check.c"
    harness.write_text(prefix + CHECKS.replace("RESET_CHOICE", str(reset_base + 3)) + init + calls, encoding="utf-8")
    if args.baseline:
        # The committed version contains the two independently observed faults.
        text = subprocess.check_output([
            "git", "-c", "safe.directory=" + str(ROOT).replace("\\", "/"),
            "show", "eaf0c720:src/cmd/ui/cmd-ui-settings.c"], cwd=ROOT, text=True)
        settings = OUT / "settings-baseline.c"
        settings.write_text(text, encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects
        if not p.endswith(("/src/main.c.obj", "/src/cmd/ui/cmd-ui-settings.c.obj"))), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(harness), str(settings),
        "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
        "-Wl,--wrap=inkey", "-Wl,--wrap=metarun_save_persistent_settings",
        "-Wl,--wrap=save_pane_config_to_json", "-o", str(exe)],
        cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="fixture-", dir=OUT) as state:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), state, args.case],
            cwd=state, env=env, check=True, timeout=20)


if __name__ == "__main__":
    main()

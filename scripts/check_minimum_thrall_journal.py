#!/usr/bin/env python3
"""Check thrall journal's brief and story action through real input routing."""
from pathlib import Path
import os
import re
import shlex
import subprocess
import tempfile
from qa9_sdl_fixture import HARNESS
from check_phone_books_layout import brace_end

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/minimum-thrall-journal"

CHECKS = r'''
#include "thrall_quest.h"
#include "ui/menu-click.h"
/* UI HELPERS */
static int input_stage, input_kind, story_count, story_buttons;
void __real_sdl_hint_quest_menu_add_button(int choice, cptr label, byte attr);
void __wrap_sdl_hint_quest_menu_add_button(int choice, cptr label, byte attr)
{
    if (choice == HINT_MESSAGE_CLICK_STORY) {
        assert(strstr(label, "Story")); ++story_buttons;
    }
    __real_sdl_hint_quest_menu_add_button(choice, label, attr);
}
void __wrap_thrall_quest_show_request_story(monster_type* monster)
{
    assert(monster == &mon_list[1] && monster->thrall_quest_requested);
    ++story_count;
}
char __wrap_inkey(void)
{
    if (input_stage++ == 0) {
        if (input_kind == 1) {
            bool wake = false;
            assert(ui_menu_click_handle_choice_action(HINT_MESSAGE_CLICK_STORY,
                UI_MENU_CLICK_PRIMARY, &wake));
            return UI_MENU_CLICK_WAKE_KEY;
        }
        if (input_kind == 2) return (char)steamdeck_secondary_key();
        return 's';
    }
    return ESCAPE;
}
static void tests(void)
{
    char goal[320]; s16b entries[4];
    op_ptr->opt[OPT_minimum_text_mode] = true;
    p_ptr->playing = true; p_ptr->depth = 5; p_ptr->py = p_ptr->px = 10;
    mon_max = 2; mon_list[1] = (monster_type){0};
    mon_list[1].r_idx = R_IDX_ALERT_HUMAN_THRALL;
    mon_list[1].thrall_quest_item = THRALL_QUEST_FLASK_OIL;
    mon_list[1].thrall_quest_requested = 1;
    mon_list[1].fy = 9; mon_list[1].fx = 10;
    assert(thrall_quest_collect(entries, 4) == 1 && entries[0] == 1);
    thrall_quest_format_goal(&mon_list[1], goal, sizeof(goal), false);
    assert(strstr(goal, "Objective:") && strstr(goal, "Reward: 75 XP"));
    mon_list[1].thrall_quest_completed = THRALL_QUEST_STATE_REWARD_PENDING;
    thrall_quest_format_goal(&mon_list[1], goal, sizeof(goal), false);
    assert(strstr(goal, "75 XP received") && strstr(goal, "return"));
    mon_list[1].thrall_quest_completed = THRALL_QUEST_STATE_REWARDED;
    thrall_quest_format_goal(&mon_list[1], goal, sizeof(goal), false);
    assert(strstr(goal, "Completed") && strstr(goal, "chosen service"));
    assert(thrall_quest_collect(entries, 4) == 1);
    op_ptr->opt[OPT_minimum_text_mode] = false;
    assert(thrall_quest_collect(entries, 4) == 0);
    op_ptr->opt[OPT_minimum_text_mode] = true;
    s32b before_turn = turn, before_playerturn = playerturn;
    u64b rng = Rand_state_export();
    for (int kind = 0; kind < 3; ++kind) {
        input_kind = kind; input_stage = 0; story_buttons = 0;
        config.input_ui_mode = kind == 2 ? SDL_INPUT_UI_MODE_CONTROLLER : SDL_INPUT_UI_MODE_PLATFORM;
        assert(steamdeck_controls_active() == (kind == 2));
        assert(thrall_quest_show_internal(1, NULL, NULL) == HINT_MESSAGE_ACTION_NONE);
        assert(input_stage == 2 && story_buttons == 2 && story_count == kind + 1);
        assert(turn == before_turn && playerturn == before_playerturn && Rand_state_export() == rng);
    }
    puts("Thrall journal: objective/reward, pending/claimed, keyboard/touch/controller story action PASS");
}
'''


def extract_function(source, name):
    match = re.search(r"(?m)^static [^\n]+\b" + name + r"\([^\n]*", source)
    assert match, name
    return source[match.start():brace_end(source, match.start())]


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    ui = (ROOT / "src/cmd/ui/cmd-ui-main-menu.c").read_text(encoding="utf-8")
    helpers = extract_function(ui, "hint_quest_tab_key") + "\n" + extract_function(ui, "hint_quest_adjacent_page")
    helpers += "\n" + ui[ui.index("typedef enum hint_message_action"):ui.index("static hint_quest_page hint_message_action_page")]
    for name in ("thrall_quest_collect", "thrall_quest_giver_name", "thrall_quest_format_goal", "thrall_quest_show_internal"):
        helpers += "\n" + extract_function(ui, name)
    fixture = HARNESS
    prefix = fixture[:fixture.index("static cptr make_path")]
    init = fixture[fixture.index("static cptr make_path"):fixture.index("    int last = ")]
    source = OUT / "check.c"
    source.write_text(prefix + CHECKS.replace("/* UI HELPERS */", helpers) + init
        + "    tests(); sdl_quit_hook(NULL); return 0;\n}\n", encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects if not p.endswith("/src/main.c.obj")), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / p) for p in
        ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    env.update(SDL_VIDEO_DRIVER="dummy", SDL_RENDER_DRIVER="software",
               SDL_AUDIO_DRIVER="dummy", CONTROL_TEST_ASSETS=str(ROOT / "lib"))
    exe = OUT / "check.exe"
    wraps = ("inkey", "thrall_quest_show_request_story", "sdl_hint_quest_menu_add_button")
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", *("-Wl,--wrap=" + name for name in wraps),
        "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="profile-", dir=OUT) as profile:
        env["CONTROL_TEST_PROFILE"] = profile
        subprocess.run([str(exe), "--windowed", "--tiles"], cwd=ROOT, env=env,
                       check=True, timeout=30)


if __name__ == "__main__":
    main()

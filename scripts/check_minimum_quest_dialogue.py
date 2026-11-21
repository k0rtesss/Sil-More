#!/usr/bin/env python3
"""Exercise the real compact quest prompt and on-demand story book."""
from pathlib import Path
import json
import os
import shlex
import subprocess
import check_quest_book_context as base

ROOT = base.ROOT
BUILD = base.BUILD
OUT = ROOT / "scripts/output/minimum-quest-dialogue"


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    harness = base.HARNESS
    harness = harness.replace('static int mode, calls, inspected, rebuilt;',
        'static int mode, calls, inspected, rebuilt, brief_calls;\n'
        'void __wrap_sdl_touch_thumb_render(void) {}')
    old = '''    assert(++calls < 2000);
    assert(sdl_character_sheet_screen_active());'''
    new = '''    assert(++calls < 2000);
    if ((mode == 4 || mode == 5) && sdl_question_menu_is_active()) {
        ++brief_calls;
        assert(strstr(g_question_menu.desc, "Objective: ") && strstr(g_question_menu.desc, "Reward: "));
        assert(g_question_menu.count == 2);
        assert(sdl_render_current_window_frame());
        capture("brief");
        return mode == 4 ? '\\r' : 's';
    }
    assert(sdl_character_sheet_screen_active());'''
    assert old in harness
    harness = harness.replace(old, new, 1)
    harness = harness.replace('if (mode == 0 || mode == 3) return ESCAPE;',
        'if (mode == 0 || mode == 3 || mode == 5) return ESCAPE;')
    harness = harness.replace('mode = test_mode; calls = inspected = rebuilt = 0; captured = false;',
        'mode = test_mode; calls = inspected = rebuilt = brief_calls = 0; captured = false;')
    harness = harness.replace('{"escape", "complete", "reward-inspect", "no-hint"}',
        '{"escape", "complete", "reward-inspect", "no-hint", "brief", "brief-story"}')
    old = '''    } else {
        quest_typewriter_menu_pages("Aulë the Smith", words, 1,
            TERM_YELLOW, TERM_WHITE, 0);
    }
    assert(!sdl_character_sheet_screen_active());'''
    new = '''    } else if (mode == 4 || mode == 5) {
        op_ptr->opt[OPT_minimum_text_mode] = true;
        quest_minimum_custom_dialogue("Aulë the Smith",
            "Objective: Forge a worthy item. Reward: Smithing and 300 XP.",
            words, 1, TERM_YELLOW, TERM_WHITE, 0);
        op_ptr->opt[OPT_minimum_text_mode] = false;
        assert(brief_calls == 1 && calls == (mode == 4 ? 1 : 2));
    } else {
        quest_typewriter_menu_pages("Aulë the Smith", words, 1,
            TERM_YELLOW, TERM_WHITE, 0);
    }
    assert(!sdl_character_sheet_screen_active());'''
    assert old in harness
    harness = harness.replace(old, new, 1)
    harness = harness.replace('for (int mode = 0; mode < 4; ++mode)',
        'for (int mode = 4; mode < 6; ++mode)')
    lines = (ROOT / "lib/edit/quest.txt").read_text(encoding="utf-8").splitlines()
    text = next(line[2:] for line in lines[lines.index("Q:2:Aulë the Smith"):] if line.startswith("I:"))
    production = (ROOT / "src/quest/quest-dialogue.c").read_text(encoding="utf-8").replace('#include "externs.h"', '')
    source = OUT / "check.c"
    source.write_text(harness.replace("/* QUEST_SOURCE */", production)
        .replace("SHIPPED_TEXT", json.dumps(text, ensure_ascii=False)), encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects if not p.endswith(
        ("/src/main.c.obj", "/src/quest/quest-dialogue.c.obj"))), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / name) for name in
        ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    wraps = ("inkey", "do_cmd_context_square_action_popup", "handle_stuff", "sdl_present_if_needed",
             "sdl_refresh_supporting_panes_layout", "sdl_suspend_main_view_zoom_for_saved_screen",
             "sdl_resume_main_view_zoom_for_saved_screen", "screen_load", "sdl_touch_thumb_render")
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", *("-Wl,--wrap=" + name for name in wraps),
        "-o", str(exe)], cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe), str(ROOT / "lib/xtra/font/EBGaramond-Regular.ttf"), str(OUT)],
        cwd=ROOT, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()

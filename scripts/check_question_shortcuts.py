#!/usr/bin/env python3
"""Exercise production question shortcuts and disabled-choice explanations.

Uses the existing Windows engine objects and a terminal input fixture; no
player saves, windows, or preferences are opened or modified.
"""
from pathlib import Path
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/question-shortcuts"
HARNESS = r'''
#include "angband.h"
#include "ui/question.h"
#include "log/log.h"
#include <assert.h>
#include "ui/question.c"

static term fixture_term;
static cptr inputs;
/* No dungeon is loaded; keep the real question loop and message log without
 * trying to repaint map features from uninitialised game templates. */
void __wrap_handle_stuff(void) { p_ptr->redraw=p_ptr->update=0; }
static errr terminal_extra(int action, int value)
{
    (void)value;
    if (action == TERM_XTRA_EVENT) {
        assert(inputs && *inputs);
        char key=*inputs++;
        if(key=='@') {
            bool wake=false;
            assert(ui_menu_click_handle_choice_action(1,UI_MENU_CLICK_PRIMARY,&wake));
            Term_keypress(UI_MENU_CLICK_WAKE_KEY);
        } else Term_keypress(key);
    }
    return 0;
}
static void ask(const ui_question_option* options, int count,
    const ui_question_button* buttons, int button_count, cptr keys, int expected)
{
    inputs = keys; Term_flush();
    bool previous_cursor = hide_cursor;
    int choice = buttons
        ? ui_question_ask_overlay_buttons("Routing", NULL, options, count,
            buttons, button_count, -1, -1, 0)
        : ui_question_ask("Routing", NULL, options, count, -1, -1, 0);
    assert(choice == expected);
    assert(!*inputs); inputs = NULL;
    assert(hide_cursor == previous_cursor);
}
static void ask_disabled(cptr keys, cptr reason, cptr expected)
{
    const ui_question_option options[]={
        {'a',"Available gift",TERM_WHITE,false},
        {'b',"Unavailable gift",TERM_WHITE,true}
    };
    const cptr reasons[]={NULL,reason};
    inputs=keys; Term_flush();
    assert(ui_question_ask_with_reasons("Reward",NULL,options,reasons,
        2,-1,-1,0)==-1);
    assert(!*inputs); inputs=NULL;
    cptr actual=message_str(0);
    size_t length=strlen(expected);
    bool matches=!strncmp(actual,expected,length)
        && (!actual[length] || !strncmp(actual+length," <",2));
    if(!matches)
        fprintf(stderr,"Disabled choice keys %s: expected '%s', got '%s'\n",
            keys,expected,actual);
    assert(matches);
}
int main(void)
{
    setbuf(stdout, NULL); log_set_level(LOG_ERROR);
    assert(SDL_Init(SDL_INIT_EVENTS));
    assert(!term_init(&fixture_term, 80, 24, 256));
    fixture_term.xtra_hook = terminal_extra;
    angband_term[0] = &fixture_term; Term_activate(&fixture_term);

    ui_question_option pairs[] = {
        { 'c', "Create object", TERM_WHITE, false },
        { 'C', "Create artefact", TERM_WHITE, false },
        { 'u', "Ordinary action", TERM_WHITE, false },
        { 'U', "Special action", TERM_WHITE, false }
    };
    ask(pairs, 4, NULL, 0, "c", 0);
    ask(pairs, 4, NULL, 0, "C", 1);
    ask(pairs, 4, NULL, 0, "u", 2);
    ask(pairs, 4, NULL, 0, "U", 3);
    pairs[1].disabled = true;
    ask(pairs, 4, NULL, 0, "C\033", -1);
    ask(pairs, 4, NULL, 0, "c", 0);
    ask(pairs, 4, NULL, 0, "2\r", 2); /* Skip disabled row. */
    ask(pairs, 4, NULL, 0, "8\r", 3);
    ask(pairs, 4, NULL, 0, "\033", -1);
    pairs[0].key = '2';
    ask(pairs, 4, NULL, 0, "2", 0); /* Explicit number beats navigation. */

    ui_question_option option[] = {{ 'c', "Option", TERM_WHITE, false }};
    ui_question_button button[] = {{ 50, 'C', "Button", TERM_WHITE, false }};
    ask(option, 1, button, 1, "c", 0);
    ask(option, 1, button, 1, "C", 50); /* Exact button beats option alias. */
    option[0].key = 'C'; button[0].key = 'c';
    ask(option, 1, button, 1, "c", 50);
    ask(option, 1, button, 1, "C", 0);
    button[0].disabled = true;
    ask(option, 1, button, 1, "c\033", -1);
    option[0].key = 'u';
    ask(option, 1, NULL, 0, "U", 0); /* Retain unique-case aliases. */
    option[0].key = 'U';
    ask(option, 1, NULL, 0, "u", 0);
    button[0].key = 'g'; button[0].disabled = false;
    ask(option, 1, button, 1, "G", 50);
    puts("Question shortcuts: c/C, u/U, row/button priority, aliases, disabled exact keys, numeric navigation, Escape and cursor restoration PASS.");
    assert(messages_init()==0);
    character_generated=true;
    cptr reason="You have no active disease to diagnose.";
    ask_disabled("b\033",reason,reason); /* Direct shortcut. */
    ask_disabled("B\033",reason,reason); /* Shortcut alias. */
    ask_disabled("b \033",reason,reason); /* Confirm grey highlight. */
    ask_disabled("@\033",reason,reason); /* Mouse/touch choice. */
    ask_disabled("b\033",NULL,"That choice is not available.");
    ask_disabled("@\033","","That choice is not available.");
    character_generated=false;
    puts("Question explanations: direct/alias shortcuts, highlighted confirmation and pointer attempts log the specific reason; empty/missing reasons retain the fallback; no disabled reward is selected PASS.");
    SDL_Quit(); return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    objects = [p for p in objects if not p.endswith(
        ("/src/main.c.obj", "/src/ui/question.c.obj"))]
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-Wl,--wrap=handle_stuff",
        "-o", str(exe)], cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe)], cwd=OUT, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()

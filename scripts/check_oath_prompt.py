#!/usr/bin/env python3
"""Exercise the actual full-screen oath prompt and its pane restoration."""
from pathlib import Path
import json
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/oath-prompt"
HARNESS = r'''
#include "angband.h"
#include "log/log.h"
#include <assert.h>
#include "support/prompt.c"
static term fixture_term;
static char answer;
static cptr expected_tail;
static cptr expected_prompt;
static errr terminal_extra(int action, int value)
{
    (void)value;
    if (action == TERM_XTRA_EVENT) {
        assert(screen_supporting_panes_hidden_active());
        assert(screen_saved_fullscreen_active());
        char visible[4096]; int used = 0;
        for (int y = 5; y < Term->hgt - 4; ++y)
            for (int x = 0; x < Term->wid; ++x) {
                char c = Term->scr->c[y][x];
                if (c != ' ' && c) visible[used++] = c;
            }
        visible[used] = 0;
        assert(strstr(visible, expected_tail));
        char normalized[4096]; int normalized_len = 0;
        for (cptr p = expected_prompt; *p; ++p)
            if (*p != ' ') normalized[normalized_len++] = *p;
        normalized[normalized_len] = 0;
        assert(streq(visible, normalized));
        Term_keypress(answer);
    }
    return 0;
}
static void ask(cptr prompt, cptr tail, int width, int height, char key, bool yes)
{
    Term_resize(width, height); Term_clear();
    Term_putstr(1, 1, -1, TERM_WHITE, "restored");
    bool hidden = screen_supporting_panes_hidden_active();
    int icky = character_icky;
    s32b saved_turn = turn, saved_playerturn = playerturn;
    expected_tail = tail; expected_prompt = prompt; answer = key; Term_flush();
    assert(get_check_oath_multiline(prompt) == yes);
    assert(screen_supporting_panes_hidden_active() == hidden);
    assert(character_icky == icky);
    assert(turn == saved_turn && playerturn == saved_playerturn);
    assert(Term->scr->c[1][1] == 'r');
}
int main(void)
{
    setbuf(stdout, NULL); log_set_level(LOG_ERROR);
    assert(SDL_Init(SDL_INIT_EVENTS));
    assert(!term_init(&fixture_term, 80, 24, 256));
    fixture_term.xtra_hook = terminal_extra;
    angband_term[0] = &fixture_term; Term_activate(&fixture_term);
    /* SHIPPED_CASES */
    ask("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA tail",
        "tail", 32, 18, ESCAPE, false);
    puts("Oath prompt: shipped Mercy/Valour wide+narrow Yes/No/Escape, long token progress, hidden panes and screen/time restoration PASS.");
    SDL_Quit(); return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prompts = {}
    current = None
    for line in (ROOT / "lib/edit/oath.txt").read_text(encoding="utf-8").splitlines():
        if line.startswith("O:"):
            current = int(line.split(":")[1])
        elif line.startswith("C:") and current in (1, 5):
            prompts[current] = line[2:]
    cases = []
    for oath, tail in ((1, "OathofMercy?"), (5, "ValorousHeart?")):
        for width, height in ((80, 24), (32, 18)):
            for key, result in (("'y'", "true"), ("'n'", "false"), ("ESCAPE", "false")):
                cases.append(f"ask({json.dumps(prompts[oath])}, {json.dumps(tail)}, {width}, {height}, {key}, {result});")
    source = OUT / "check.c"
    source.write_text(HARNESS.replace("/* SHIPPED_CASES */", "\n".join(cases)), encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    objects = [p for p in objects if not p.endswith(("/src/main.c.obj", "/src/support/prompt.c.obj"))]
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-o", str(exe)], cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe)], cwd=OUT, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Run production quest book loops and render their floor-context ownership.

No saves or preferences are loaded. A pending floor object is represented by a
fixed fixture; the popup builder is wrapped so the test can isolate book UI.
The real SDL question menu, parchment renderer, book loops and screen stack run.
"""
from pathlib import Path
import json
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/quest-book-context"
HARNESS = r'''
#include "angband.h"
#include "sdl/main-sdl-private.h"
#include "log/log.h"
#include <assert.h>
/* QUEST_SOURCE */
static object_type pending_floor;
static object_type floor_before;
static cptr output_dir, case_name;
static int mode, calls, inspected, rebuilt;
static bool captured;
static char inspect_name[128];
static errr terminal_extra(int action, int value)
{
    (void)value;
    if (action == TERM_XTRA_EVENT) Term_keypress(ESCAPE);
    return 0;
}

bool __wrap_do_cmd_context_square_action_popup(void)
{
    assert(!sdl_character_sheet_screen_active());
    ++rebuilt;
    if (!pending_floor.number) return false;
    sdl_question_menu_begin("Pending floor item");
    sdl_question_menu_set_anchor_grid(p_ptr->py, p_ptr->px);
    sdl_question_menu_add_button('x', "Description", TERM_L_BLUE);
    sdl_question_menu_add_button('g', "Pick Up", TERM_L_WHITE);
    sdl_question_menu_finish();
    sdl_question_menu_set_context_hint();
    return true;
}
void __wrap_handle_stuff(void) {}
void __wrap_sdl_present_if_needed(sdl_view* view) { (void)view; }
/* The fixture owns its one fixed-size canvas, without gameplay pane rebuilds. */
void __wrap_sdl_refresh_supporting_panes_layout(void) {}
void __wrap_sdl_suspend_main_view_zoom_for_saved_screen(void) {}
bool __wrap_sdl_resume_main_view_zoom_for_saved_screen(void) { return false; }
void __wrap_screen_load(void) { screen_load_quiet(); }
static void capture(cptr suffix)
{
    char path[1024];
    SDL_Surface* pixels = SDL_RenderReadPixels(g_state.renderer, NULL);
    assert(pixels);
    strnfmt(path, sizeof(path), "%s/%s-%s.png", output_dir, case_name, suffix);
    assert(IMG_SavePNG(pixels, path));
    SDL_DestroySurface(pixels);
}
char __wrap_inkey(void)
{
    assert(++calls < 2000);
    assert(sdl_character_sheet_screen_active());
    assert(!sdl_question_menu_context_hint_active());
    assert(sdl_render_current_window_frame());
    if (!captured) { capture("book"); captured = true; }
    if (mode == 0 || mode == 3) return ESCAPE;
    if (mode == 2) return inspected ? ESCAPE : 'x';
    return ' ';
}
static void inspect_reward(int value)
{
    assert(value == 42);
    assert(!sdl_character_sheet_screen_active());
    ++inspected;
    /* An inspection callback can rebuild a gameplay hint before returning. */
    assert(__wrap_do_cmd_context_square_action_popup());
    captured = false;
    strnfmt(inspect_name, sizeof(inspect_name), "%s-return", case_name);
    case_name = inspect_name;
}
static void run(int width, int height, int test_mode, bool hint)
{
    sdl_view* view = &g_views[PANE_MAIN];
    mode = test_mode; calls = inspected = rebuilt = 0; captured = false;
    const char* names[] = {"escape", "complete", "reward-inspect", "no-hint"};
    char name[96];
    strnfmt(name, sizeof(name), "%s-%dx%d", names[mode], width, height);
    printf("Quest book fixture %s\n", name);
    case_name = name;
    g_state.window = SDL_CreateWindow("Quest book fixture", width, height, SDL_WINDOW_HIDDEN);
    assert(g_state.window);
    g_state.renderer = SDL_CreateRenderer(g_state.window, "software");
    assert(g_state.renderer);
    g_state.safe_area = (SDL_Rect){0, 0, width, height};
    assert(!term_init(&view->t, width / 8, height / 16, 256));
    view->t.xtra_hook = terminal_extra;
    Term_activate(&view->t); term_screen = &view->t; angband_term[0] = &view->t;
    view->term_ready = true; view->rect = (SDL_Rect){0, 0, width, height};
    view->cell_w = 8; view->cell_h = 16; view->cols = width / 8; view->rows = height / 16;
    view->canvas = SDL_CreateTexture(g_state.renderer, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET, width, height);
    assert(view->canvas);
    pending_floor = (object_type){.k_idx=1, .number=1, .tval=TV_FOOD};
    floor_before = pending_floor;
    if (hint) assert(__wrap_do_cmd_context_square_action_popup());
    else sdl_question_menu_clear_context_hint();
    int initial_rebuilt = rebuilt;
    s32b old_turn = turn, old_playerturn = playerturn, old_xp = p_ptr->new_exp;
    cptr words[] = { SHIPPED_TEXT };
    if (mode == 2) {
        const int values[] = {42}; cptr labels[] = {"Gift"};
        assert(quest_reward_book_choice("Aulë's reward", words, 1, "Choose your gift:",
            values, labels, NULL, 1, 0, inspect_reward) == -1);
        assert(inspected == 1);
    } else {
        quest_typewriter_menu_pages("Aulë the Smith", words, 1,
            TERM_YELLOW, TERM_WHITE, 0);
    }
    assert(!sdl_character_sheet_screen_active());
    assert(!screen_supporting_panes_hidden_active());
    assert(!character_icky);
    assert(sdl_question_menu_context_hint_active() == hint);
    assert(rebuilt == initial_rebuilt + (hint ? 1 : 0) + inspected);
    assert(!memcmp(&pending_floor, &floor_before, sizeof(pending_floor)));
    assert(turn == old_turn && playerturn == old_playerturn && p_ptr->new_exp == old_xp);
    /* Restored-hint rendering needs a live dungeon; GameControl replay covers
     * that. This isolated renderer checks book pixels and hint ownership. */
    sdl_question_menu_clear();
    sdl_story_font_cache_clear();
    SDL_DestroyTexture(view->canvas); view->canvas = NULL;
    term_nuke(&view->t); view->term_ready = false;
    Term = NULL; term_screen = NULL; angband_term[0] = NULL;
    SDL_DestroyRenderer(g_state.renderer); g_state.renderer = NULL;
    SDL_DestroyWindow(g_state.window); g_state.window = NULL;
}
int main(int argc, char** argv)
{
    assert(argc == 3); output_dir = argv[2];
    setbuf(stdout, NULL);
    player_type player = {0}; player_other options = {0}; maxima limits = {0};
    p_ptr = &player; op_ptr = &options; z_info = &limits;
    p_ptr->playing = true; p_ptr->py = 7; p_ptr->px = 8;
    p_ptr->cur_map_hgt = 32; p_ptr->cur_map_wid = 64; p_ptr->new_exp = 123;
    character_generated = character_dungeon = true;
    log_set_quiet(true); SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)); assert(TTF_Init());
    sdl_config_set_defaults(&config); sdl_sync_palette(); config.use_unsafe_area = true;
    g_state.system_scale = 1;
    SDL_strlcpy(config.story_font, argv[1], sizeof(config.story_font));
    SDL_strlcpy(config.story_font2, argv[1], sizeof(config.story_font2));
    for (int mode = 0; mode < 4; ++mode) {
        run(768, 576, mode, mode != 3);
        run(580, 900, mode, mode != 3);
    }
    puts("Quest book context: actual SDL Escape/page completion/reward inspection, restored hint, preserved floor object/XP/turns PASS.");
    TTF_Quit(); SDL_Quit(); return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    lines = (ROOT / "lib/edit/quest.txt").read_text(encoding="utf-8").splitlines()
    text = next(line[2:] for line in lines[lines.index("Q:2:Aulë the Smith"):] if line.startswith("I:"))
    source = OUT / "check.c"
    # The SDL private header already includes externs.h, which has no guard.
    production = (ROOT / "src/quest/quest-dialogue.c").read_text().replace('#include "externs.h"', '')
    harness = HARNESS.replace("/* QUEST_SOURCE */", production)
    source.write_text(harness.replace("SHIPPED_TEXT", json.dumps(text, ensure_ascii=False)), encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    objects = [p for p in objects if not p.endswith(("/src/main.c.obj", "/src/quest/quest-dialogue.c.obj"))]
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-Wl,--wrap=inkey,--wrap=do_cmd_context_square_action_popup,--wrap=handle_stuff,--wrap=sdl_present_if_needed,--wrap=sdl_refresh_supporting_panes_layout,--wrap=sdl_suspend_main_view_zoom_for_saved_screen,--wrap=sdl_resume_main_view_zoom_for_saved_screen,--wrap=screen_load",
        "-o", str(exe)], cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe), str(ROOT / "lib/xtra/font/EBGaramond-Regular.ttf"), str(OUT)],
        cwd=ROOT, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()

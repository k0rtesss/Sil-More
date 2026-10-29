#!/usr/bin/env python3
"""Exercise real SDL selection input and presentation in an isolated profile.

Build build-standard first. A linker wrapper observes real presentations and
queues mouse events; no player saves or live desktop input are used.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

import check_game_control as base

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/birth-selection-response"

HELPERS = r'''
typedef struct birth_menu { bool ghost; cptr name; cptr text; } birth_menu;
typedef struct birth_select_page {
    cptr title, frame_top, intro, frame_bottom;
    cptr *group_headings;
    int detail_stat_rows_hint, detail_ability_rows_hint, detail_trait_rows_hint;
    bool open_on_choice_page;
} birth_select_page;
extern int get_player_choice(birth_menu*, int, int, void (*)(birth_menu),
    const birth_select_page*);

static bool driving;
static int click_stage, stale_presents, presented, changed_frames;
static cptr expected_description;

void __real_sdl_present_if_needed(sdl_view* view);
void __wrap_sdl_present_if_needed(sdl_view* view)
{
    bool painting = g_state.need_present;
    if (driving && painting && ui_menu_click_has_pending()
        && Term->key_head != Term->key_tail)
        stale_presents++;
    __real_sdl_present_if_needed(view);
    if (!painting) return;
    presented++;
    if (!driving || !sdl_character_sheet_screen_active()) return;

    if (click_stage == 1
        && g_sdl_character_sheet_screen.focus_choice == 1
        && streq(g_sdl_character_sheet_screen.select_description,
            expected_description))
    {
        changed_frames++;
        click_stage = 2;
    }
    else if (click_stage != 0) return;

    for (int i = 0; i < g_sdl_character_sheet_screen.hit_count; i++) {
        sdl_character_sheet_hit* hit = &g_sdl_character_sheet_screen.hits[i];
        if (hit->choice != 1) continue;
        SDL_Event event = {0};
        event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
        event.button.windowID = SDL_GetWindowID(g_state.window);
        event.button.button = SDL_BUTTON_LEFT;
        event.button.down = true;
        event.button.clicks = 1;
        event.button.x = hit->rect.x + hit->rect.w / 2;
        event.button.y = hit->rect.y + hit->rect.h / 2;
        assert(SDL_PushEvent(&event));
        if (click_stage == 0) click_stage = 1;
        else click_stage = 3;
        return;
    }
    assert(!"selection row was not painted");
}

void __wrap_birth_coach_show_once(int stage) { (void)stage; }

static void detail(birth_menu choice)
{
    sdl_character_sheet_screen_add_select_detail(choice.name, TERM_WHITE, "");
}

static void selection_case(bool book)
{
    birth_menu choices[] = {
        {false, "First", "First description."},
        {false, "Second", "Second description."}
    };
    birth_select_page page = {"Selection response", "A beginning.",
        book ? "A short chronicle. Another sentence. And a third." : NULL,
        "A charge.", NULL, 0, 0, 0, true};
    click_stage = stale_presents = changed_frames = 0;
    expected_description = choices[1].text;
    driving = true;
    int result = get_player_choice(choices, 2, 0, book ? NULL : detail, &page);
    driving = false;
    assert(result == 1 && click_stage == 3 && changed_frames == 1);
    assert(stale_presents == 0);
    sdl_character_sheet_screen_hide();
    ui_menu_click_clear();
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    printf("%s: first click updates description, second confirms, no stale presentation: PASS\n",
        book ? "Race book" : "Hero picker");
}

static void settings_case(void)
{
    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(false);
    sdl_character_sheet_screen_begin_select(0,"Settings selection");
    sdl_character_sheet_screen_set_select_menu_style(true);
    sdl_character_sheet_screen_add_select_row(0,"First",TERM_WHITE,"");
    sdl_character_sheet_screen_add_select_row(1,"Second",TERM_WHITE,"");
    sdl_character_sheet_screen_set_select_description("First description.");
    assert(sdl_character_sheet_screen_commit_select(0));
    click_stage = stale_presents = changed_frames = 0;
    expected_description = "Second description.";
    driving = true;
    int choice=-1, action=-1;
    assert(inkey()=='\r');
    assert(ui_menu_click_take_action(&choice,&action));
    assert(choice==1 && action==UI_MENU_CLICK_PRIMARY);
    sdl_character_sheet_screen_set_select_description(expected_description);
    assert(sdl_character_sheet_screen_commit_select(1));
    assert(inkey()=='\r');
    assert(ui_menu_click_take_action(&choice,&action));
    assert(choice==1 && action==UI_MENU_CLICK_PRIMARY);
    driving = false;
    assert(click_stage==3 && changed_frames==1 && stale_presents==0);
    sdl_character_sheet_screen_hide();
    ui_menu_click_clear();
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    puts("Settings selection updates before presentation: PASS");
}

static void pointer_only_case(void)
{
    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(false);
    sdl_character_sheet_screen_begin_select(0, "Pointer-only redraw");
    sdl_character_sheet_screen_set_select_menu_style(true);
    sdl_character_sheet_screen_add_select_row(0,"First",TERM_WHITE,"");
    sdl_character_sheet_screen_add_select_row(1,"Second",TERM_WHITE,"");
    assert(sdl_character_sheet_screen_commit_select(0));
    sdl_present_if_needed(sdl_view_from_term(Term));
    SDL_FRect row = {0};
    for (int i=0; i<g_sdl_character_sheet_screen.hit_count; i++)
        if (g_sdl_character_sheet_screen.hits[i].choice == 1)
            row = g_sdl_character_sheet_screen.hits[i].rect;
    assert(row.w > 0 && row.h > 0);
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    SDL_Event event = {0};
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.windowID = SDL_GetWindowID(g_state.window);
    event.motion.x = row.x + row.w / 2;
    event.motion.y = row.y + row.h / 2;
    assert(SDL_PushEvent(&event));
    int before = presented;
    assert(callback_sdl_xtra(TERM_XTRA_EVENT, 0) == 0);
    assert(presented > before && !ui_menu_click_has_pending());
    sdl_character_sheet_screen_hide();
    ui_menu_click_clear();
    puts("Pointer-only hover still presents without a queued selection: PASS");
}
'''

CASES = r'''
    sdl_welcome_screen_hide();
    screen_set_startup_supporting_panes_hidden(false);
    screen_set_startup_touch_pane_hidden(false);
    selection_case(true);
    selection_case(false);
    settings_case();
    pointer_only_case();
    sdl_control_shutdown();
    return 0;
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    harness = base.HARNESS.replace('static cptr make_path(', HELPERS + '\nstatic cptr make_path(', 1)
    harness = harness.replace('    if (SDL_getenv("CONTROL_TEST_INTERACTIVE")) {',
                              CASES + '\n    if (SDL_getenv("CONTROL_TEST_INTERACTIVE")) {', 1)
    source = OUT / "check.c"
    source.write_text(harness, encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects
                                   if not obj.endswith('/src/main.c.obj')), encoding="utf-8")
    env = os.environ.copy()
    env.pop("SIL_MORE_CONTROL_DIR", None)
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / p) for p in
        ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    env.update(SDL_VIDEO_DRIVER="dummy", SDL_RENDER_DRIVER="software",
               SDL_AUDIO_DRIVER="dummy", CONTROL_TEST_ASSETS=str(ROOT / "lib"))
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp",
        "-Wl,--wrap=sdl_present_if_needed,--wrap=birth_coach_show_once",
        "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="profile-", dir=OUT) as profile:
        env["CONTROL_TEST_PROFILE"] = profile
        subprocess.run([str(exe), "--windowed"], cwd=ROOT, env=env,
                       check=True, timeout=45)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Replay Chronicle touch scrolling with the real mobile SDL renderer.

Uses an isolated offscreen window, real fonts, and the production event
dispatcher. No player saves or configuration are opened.
"""
from pathlib import Path
import os
import shlex
import subprocess

from check_phone_welcome_poetry import HARNESS as WELCOME, INIT

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/chronicle-scroll"
PREFIX = WELCOME[:WELCOME.index("static void indicator(void)")]

HARNESS = PREFIX + r'''
enum { CONFIRM = 500, CANCEL, DIFFICULTY = 600 };
static void render_book(void)
{
    SDL_Rect canvas={0,0,fixture_width,fixture_height};
    g_sdl_narrative_portrait_rendering=fixture_height>fixture_width;
    sdl_character_sheet_screen_render_canvas(&canvas);
    g_sdl_narrative_portrait_rendering=false;
}
static void build_book(bool confirm)
{
    static cptr chapters[]={"I. Statistics","II. Blessings","III. Curses",
        "IV. Difficulty","V. Tales"};
    sdl_character_sheet_screen_begin_book("The Chronicle of the Long Defiance");
    sdl_character_sheet_screen_set_book_close_button(true);
    sdl_character_sheet_screen_set_book_close_label("Proceed to character creation");
    for(int i=0;i<5;i++)
        sdl_character_sheet_screen_add_book_contents(chapters[i],200+i,i);
    for(int chapter=0;chapter<5;chapter++) {
        if(chapter) sdl_character_sheet_screen_break_book_page();
        sdl_character_sheet_screen_add_book_paragraph_colored(
            chapter==3?"IV - The Weight of Doom":chapters[chapter],TERM_YELLOW);
        if(chapter==3 && confirm) {
            sdl_character_sheet_screen_add_book_paragraph_colored(
                "Change Echoes (Normal) to Echoes (Hard)?",TERM_L_GREEN);
            sdl_character_sheet_screen_add_book_paragraph(
                "4 starting curse stacks across 4 curses. Win at 15 Silmarils. "
                "Blessings require 400/500/650 fallen-score points on "
                "easier/normal/harder thresholds.");
            sdl_character_sheet_screen_add_book_paragraph_colored(
                "WARNING: This difficulty increase is permanent for this tale. "
                "You cannot return to the current difficulty after confirming.",TERM_L_RED);
            sdl_character_sheet_screen_add_book_action_colored(
                "Confirm permanent difficulty change",CONFIRM,TERM_L_RED);
            sdl_character_sheet_screen_add_book_action_colored(
                "Cancel - keep current difficulty",CANCEL,TERM_L_GREEN);
        } else {
            for(int row=0;row<12;row++) {
                char text[256];
                strnfmt(text,sizeof(text),"%d. %s - %s",row+1,
                    chapter==3?"Echoes (Hard)":"The long road through Beleriand",
                    "Four starting curse stacks across four curses. "
                    "Every choice and its full description must be reachable.");
                sdl_character_sheet_screen_add_book_action(text,DIFFICULTY+chapter*20+row);
            }
        }
    }
    sdl_character_sheet_screen_commit_book(); render_book();
}
static void finger(Uint32 type,float x,float y)
{
    SDL_Event ev={0}; ev.type=type;
    ev.tfinger.windowID=SDL_GetWindowID(g_state.window);
    ev.tfinger.touchID=1; ev.tfinger.fingerID=7;
    ev.tfinger.x=x/fixture_width; ev.tfinger.y=y/fixture_height;
    sdl_handle_event(&g_state,&ev);
}
static void drag(bool down)
{
    SDL_FRect view=g_sdl_character_sheet_screen.select_scroll_rect;
    float x=view.x+view.w*.5f;
    float first=view.y+view.h*(down?.85f:.15f);
    float last=view.y+view.h*(down?.15f:.85f);
    finger(SDL_EVENT_FINGER_DOWN,x,first);
    finger(SDL_EVENT_FINGER_MOTION,x,last);
    finger(SDL_EVENT_FINGER_UP,x,last);
    render_book();
    char key=0;
    fixture_assert(Term_inkey(&key,false,true)!=0);
    fixture_assert(!ui_menu_click_has_pending());
    fixture_assert(sdl_character_sheet_screen_active());
}
static bool hit_visible(int choice)
{
    SDL_FRect view=g_sdl_character_sheet_screen.select_scroll_rect;
    for(int i=0;i<g_sdl_character_sheet_screen.hit_count;i++) {
        sdl_character_sheet_hit *hit=&g_sdl_character_sheet_screen.hits[i];
        if(hit->choice!=choice) continue;
        fixture_assert(hit->rect.x>=view.x-1 && hit->rect.y>=view.y-1);
        fixture_assert(hit->rect.x+hit->rect.w<=view.x+view.w+1);
        fixture_assert(hit->rect.y+hit->rect.h<=view.y+view.h+1);
        return hit->rect.h>=sdl_ui_min_tap_px()*.5f;
    }
    return false;
}
static void check_chapters(bool confirm)
{
    build_book(confirm);
    ui_menu_click_begin();
    fixture_assert(sdl_character_sheet_screen_select_page_count()==5);
    if(confirm) {
        fixture_assert(sdl_character_sheet_screen_book_action_page(CONFIRM)==3);
        fixture_assert(sdl_character_sheet_screen_book_action_page(CANCEL)==3);
    }
    for(int chapter=0;chapter<5;chapter++) {
        sdl_character_sheet_screen_set_book_page(chapter); render_book();
        if(chapter==3)
            capture(confirm?"confirmation-before-scroll":"choices-before-scroll");
        fixture_assert(g_sdl_character_sheet_screen.sheet_scroll==0);
        SDL_FRect view=g_sdl_character_sheet_screen.select_scroll_rect;
        inside(view);
        fixture_assert(view.h>=sdl_ui_role_font_px(SDL_UI_FONT_BODY)*2);
        int final=chapter==3 && confirm?CANCEL:DIFFICULTY+chapter*20+11;
        int seen=0;
        for(int attempt=0;attempt<200;attempt++) {
            for(int row=0;row<12;row++)
                if(hit_visible(DIFFICULTY+chapter*20+row)) seen|=1<<row;
            if(g_sdl_character_sheet_screen.sheet_scroll
                ==g_sdl_character_sheet_screen.sheet_scroll_max) break;
            int old=g_sdl_character_sheet_screen.sheet_scroll;
            drag(true);
            fixture_assert(g_sdl_character_sheet_screen.sheet_scroll>old);
        }
        fixture_assert(g_sdl_character_sheet_screen.sheet_scroll
            ==g_sdl_character_sheet_screen.sheet_scroll_max);
        fixture_assert(hit_visible(final));
        if(!(chapter==3 && confirm)) fixture_assert(seen==4095);
        if(chapter==3) {
            capture(confirm?"confirmation-scrolled":"choices-scrolled");
            if(confirm) {
                const sdl_character_sheet_hit* hit=NULL;
                for(int i=0;i<g_sdl_character_sheet_screen.hit_count;i++)
                    if(g_sdl_character_sheet_screen.hits[i].choice==CANCEL)
                        hit=&g_sdl_character_sheet_screen.hits[i];
                fixture_assert(hit);
                float x=hit->rect.x+hit->rect.w*.5f,y=hit->rect.y+hit->rect.h*.5f;
                finger(SDL_EVENT_FINGER_DOWN,x,y); finger(SDL_EVENT_FINGER_UP,x,y);
                int clicked=-1,action=-1; char key=0;
                fixture_assert(ui_menu_click_take_action(&clicked,&action));
                fixture_assert(clicked==CANCEL && action==UI_MENU_CLICK_PRIMARY);
                fixture_assert(Term_inkey(&key,false,true)==0);
                ui_menu_click_clear();
                ui_menu_click_begin();
            }
        }
        for(int attempt=0;g_sdl_character_sheet_screen.sheet_scroll>0 && attempt<200;attempt++)
            drag(false);
        fixture_assert(g_sdl_character_sheet_screen.sheet_scroll==0);
    }
    sdl_character_sheet_screen_hide(); ui_menu_click_clear();
}
''' + INIT + r'''
    g_state.palette[TERM_L_BLUE]=(SDL_Color){0,230,230,255};
    g_state.palette[TERM_L_GREEN]=(SDL_Color){0,255,0,255};
    g_state.palette[TERM_L_RED]=(SDL_Color){255,95,95,255};
    g_state.palette[TERM_YELLOW]=(SDL_Color){255,255,0,255};
    g_sdl_blocking_key_wait=true; fixture.active=false;
    for(int big=0;big<2;big++) {
        config.bigger_font=big;
        for(int menu=0;menu<SDL_MENU_FONT_COUNT;menu++) config.menu_bigger_font[menu]=big;
        check_chapters(false); check_chapters(true);
    }
    printf("Chronicle %dx%d @%.3f: all chapters, touch scrolling, final actions and Cancel PASS\n",
        fixture_width,fixture_height,fixture_density);
    sdl_ui_text_cache_clear(); sdl_story_font_cache_clear(); term_nuke(&view->t);
    SDL_DestroyRenderer(g_state.renderer); SDL_DestroyWindow(g_state.window); TTF_Quit(); SDL_Quit();
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/sdl/ui/sdl-gameplay-tutorial.c.obj",
                "/src/sdl/input/sdl-touch-tutorial.c.obj", "/src/sdl/render/sdl-fonts.c.obj",
                "/src/sdl/ui/sdl-song-menu.c.obj", "/src/sdl/ui/sdl-question-menu.c.obj",
                "/src/sdl/ui/sdl-screens.c.obj", "/src/sdl/input/sdl-screen-pointer.c.obj")
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects if not obj.endswith(excluded)))
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(
        [str(BUILD / "_deps" / dep) for dep in ["SDL", "SDL_ttf", "SDL_image", "SDL_mixer"]]
        + ["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    wraps = ["tutorial_get_view", "tutorial_is_active", "tutorial_revision", "get_sdl_gameplay_tutorial_mode",
             "SDL_WaitEvent", "sdl_touch_round_layer_controls_active", "sdl_touch_round_compute_layout",
             "sdl_touch_thumb_current_bounds", "sdl_map_grid_cell_rect", "sdl_touch_only_device_active",
             "sdl_touch_only_mobile_device_active", "sdl_get_layout_screen_rect", "sdl_overlay_pane_anchor_rect",
             "SDL_GetDisplayContentScale", "sdl_terminal_menu_font_px", "sdl_mobile_lifecycle_handle_event",
             "sdl_touch_tutorial_device_available", "SDL_RenderTexture", "sdl_touch_pane_point_to_slot"]
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
                    "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-Wl," + ",".join("--wrap=" + w for w in wraps),
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    for width, height, density in [(580, 1280, 1.667), (360, 800, 1), (800, 360, 1),
                                    (720, 1600, 2), (1080, 2340, 2.75), (2340, 1080, 2.75)]:
        subprocess.run([str(exe), str(width), str(height), str(density),
                        str(ROOT / "lib/xtra/font/EBGaramond-Regular.ttf"),
                        str(ROOT / "lib/xtra/font/Cinzel-Medium.ttf"),
                        str(ROOT / "lib/xtra/font/VictorMono-Medium.ttf")],
                       cwd=OUT, env=env, check=True, timeout=60)


if __name__ == "__main__":
    main()

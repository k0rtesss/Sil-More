#!/usr/bin/env python3
"""Replay native Options input and nested skill-allocation gesture ownership.

Uses the configured Windows build and the mobile character-screen code with
catalogue-derived tutorial views. No player saves or settings are opened.
"""
from pathlib import Path
import json
import os
import shlex
import subprocess

from check_gameplay_tutorial_render import HARNESS as CARD_HARNESS

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/options-tutorial-check"

# Reuse the existing card/font instrumentation and injected tutorial view.
HARNESS = CARD_HARNESS[:CARD_HARNESS.index("static void field(")] + r'''
/* Compile the actual mobile scrolling/tap branches against desktop SDL. */
#undef SIL_SDL_MOBILE_BUILD
#define SIL_SDL_MOBILE_BUILD 1
#include "sdl/ui/sdl-screens.c"

static int continued;
void __wrap_tutorial_continue(void) { ++continued; fixture.active=false; }
bool __wrap_sdl_touch_only_device_active(void) { return true; }
SDL_Rect __wrap_sdl_get_layout_screen_rect(void)
{ return (SDL_Rect){0,0,fixture_width,fixture_height}; }

static void touch(SDL_EventType type, float x, float y)
{
    SDL_Event ev={0};
    ev.type=type;
    ev.tfinger.timestamp=SDL_GetTicksNS();
    ev.tfinger.windowID=SDL_GetWindowID(g_state.window);
    ev.tfinger.touchID=1; ev.tfinger.fingerID=7;
    ev.tfinger.x=x/fixture_width; ev.tfinger.y=y/fixture_height;
    sdl_handle_event(&g_state,&ev);
}

static void tap(SDL_FRect r)
{
    float x=r.x+r.w/2, y=r.y+r.h/2;
    touch(SDL_EVENT_FINGER_DOWN,x,y);
    touch(SDL_EVENT_FINGER_MOTION,x+1,y+1);
    touch(SDL_EVENT_FINGER_UP,x+1,y+1);
}

static SDL_FRect hit_rect(int choice)
{
    for (int i=0;i<g_sdl_character_sheet_screen.hit_count;++i)
        if (g_sdl_character_sheet_screen.hits[i].choice==choice)
            return g_sdl_character_sheet_screen.hits[i].rect;
    fixture_assert(false);
    return (SDL_FRect){0};
}

static void frame(void)
{
    drawn_count=0; drawn_content[0]='\0';
    fixture_assert(sdl_render_current_window_frame());
}

static void menu(int rows)
{
    static const char *labels[]={"Input Options", "General Settings",
        "Interface Options", "Visual Options", "Text Options",
        "Gameplay Options", "Sound Options", "Other Options", "Return to Game"};
    ui_menu_click_begin(); ui_menu_click_set_hover_enabled(true);
    sdl_character_sheet_screen_begin_select(1,"Options");
    sdl_character_sheet_screen_set_select_menu_style(true);
    for (int i=0;i<rows;++i)
        sdl_character_sheet_screen_add_select_row(i+1,labels[i%9],
            i==0?TERM_L_BLUE:TERM_WHITE,"");
    sdl_character_sheet_screen_set_select_description(
        "Keyboard, controller, and mouse input settings and tutorials.");
    fixture_assert(sdl_character_sheet_screen_commit_select(1));
    frame();
}

static void expect_click(int choice)
{
    int actual=0, action=0;
    char key=0;
    fixture_assert(ui_menu_click_take_action(&actual,&action));
    fixture_assert(actual==choice && action==UI_MENU_CLICK_PRIMARY);
    fixture_assert(Term_inkey(&key,false,true)==0);
    fixture_assert(key==(choice==-1?ESCAPE:'\r'));
    fixture_assert(Term_inkey(&key,false,true)!=0);
}

static void expect_secondary(int choice)
{
    int actual=0, action=0;
    char key=0;
    fixture_assert(ui_menu_click_take_action(&actual,&action));
    fixture_assert(actual==choice && action==UI_MENU_CLICK_SECONDARY);
    fixture_assert(Term_inkey(&key,false,true)==0 && key=='\r');
    fixture_assert(Term_inkey(&key,false,true)!=0);
}

static void check_allocation_gestures(void)
{
    int old_base[S_MAX]={0}, gains[S_MAX]={0}, costs[S_MAX]={0};
    char key=0;
    SDL_strlcpy(fixture_id,"skills.native.long-tap",sizeof(fixture_id));
    /* Both parent browsers keep their Back scopes while training skills. */
    sdl_screen_back_gesture_begin();
    sdl_screen_back_gesture_begin();
    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(true);
    sdl_character_sheet_screen_show_birth_skills(old_base,gains,costs,S_MEL,1000);
    /* Isolate input ownership from layout: register a native allocation row. */
    SDL_FRect row={20,fixture_height/2.0f,fixture_width-40,40};
    g_sdl_character_sheet_screen.hit_count=1;
    g_sdl_character_sheet_screen.hits[0]=(sdl_character_sheet_hit){
        .rect=row,.choice=S_MEL};
    float x=row.x+row.w/2, y=row.y+row.h/2;
    touch(SDL_EVENT_FINGER_DOWN,x,y);
    fixture_assert(g_sdl_character_sheet_screen.touch_press.active);
    g_sdl_character_sheet_screen.touch_press.start_time-=600000000ULL;
    if (g_screen_back_touch_press.active)
        g_screen_back_touch_press.start_time-=600000000ULL;
    /* Exercise the same timer flush that closed the screen on Android. */
    fixture_assert(!sdl_screen_back_gesture_flush_pending_press(SDL_GetTicksNS()));
    fixture_assert(Term_inkey(&key,false,true)!=0);
    touch(SDL_EVENT_FINGER_UP,x,y);
    expect_secondary(S_MEL);
    fixture_assert(sdl_character_sheet_screen_active());

    /* Right-click uses the same refund action and cannot leak Escape on up. */
    SDL_strlcpy(fixture_id,"skills.native.right-click",sizeof(fixture_id));
    SDL_Event ev={0};
    ev.type=SDL_EVENT_MOUSE_BUTTON_DOWN;
    ev.button.timestamp=SDL_GetTicksNS();
    ev.button.windowID=SDL_GetWindowID(g_state.window);
    ev.button.which=1; ev.button.button=SDL_BUTTON_RIGHT;
    ev.button.x=x; ev.button.y=y;
    /* Route through the two real owners without unrelated mouse-cursor art. */
    fixture_assert(!sdl_screen_back_gesture_handle_event(&ev));
    fixture_assert(sdl_character_sheet_screen_handle_event(&ev));
    expect_secondary(S_MEL);
    ev.type=SDL_EVENT_MOUSE_BUTTON_UP;
    ev.button.timestamp=SDL_GetTicksNS();
    fixture_assert(!sdl_screen_back_gesture_handle_event(&ev));
    fixture_assert(sdl_character_sheet_screen_handle_event(&ev));
    fixture_assert(Term_inkey(&key,false,true)!=0);

    /* A parent press already armed before allocation cannot cancel it. */
    g_screen_back_touch_press.active=true;
    g_screen_back_touch_press.start_time=SDL_GetTicksNS()-600000000ULL;
    fixture_assert(sdl_screen_back_gesture_pending_timeout_ms(SDL_GetTicksNS())<0);
    fixture_assert(!g_screen_back_touch_press.active);
    fixture_assert(!sdl_screen_back_gesture_flush_pending_press(SDL_GetTicksNS()));
    sdl_character_sheet_screen_hide();
    ui_menu_click_clear();

    /* Closing allocation restores the parent's original Back gesture. */
    menu(9);
    touch(SDL_EVENT_FINGER_DOWN,x,y);
    g_screen_back_touch_press.start_time-=600000000ULL;
    fixture_assert(sdl_screen_back_gesture_flush_pending_press(SDL_GetTicksNS()));
    fixture_assert(Term_inkey(&key,false,true)==0 && key==ESCAPE);
    touch(SDL_EVENT_FINGER_UP,x,y);
    fixture_assert(Term_inkey(&key,false,true)!=0);
    sdl_screen_back_gesture_end();
    sdl_screen_back_gesture_end();
    fixture_assert(!sdl_screen_back_gesture_active());
    sdl_character_sheet_screen_hide();
    ui_menu_click_clear();
}

static void check(int width,int height,cptr body)
{
    fixture_width=width; fixture_height=height; fixture_font=24;
    SDL_strlcpy(fixture_id,"menu.settings.native",sizeof(fixture_id));
    config.aux_view_font_size=24;
    g_state.window=SDL_CreateWindow("Options tutorial fixture",width,height,SDL_WINDOW_HIDDEN);
    fixture_assert(g_state.window!=NULL);
    g_state.renderer=SDL_CreateRenderer(g_state.window,"software");
    fixture_assert(g_state.renderer!=NULL);
    g_state.safe_area=(SDL_Rect){0,0,width,height};
    sdl_view *view=&g_views[PANE_MAIN];
    term_init(&view->t,80,24,256);
    Term_activate(&view->t); term_screen=&view->t;
    character_icky=1;
    fixture=(tutorial_view){.active=true,.can_continue=true,
        .kind=TUTORIAL_STEP_INFO,.step=1,.step_count=1,.revision=1};
    SDL_strlcpy(fixture.id,"menu.settings",sizeof(fixture.id));
    SDL_strlcpy(fixture.title,"Settings",sizeof(fixture.title));
    SDL_strlcpy(fixture.body,body,sizeof(fixture.body));
    SDL_strlcpy(fixture.anchor,"menu",sizeof(fixture.anchor));
    tutorial_last_input=TUTORIAL_INPUT_TOUCH;
    tutorial_force_sync=true; sdl_gameplay_tutorial_sync();
    menu(9);
    if (width==580) {
        SDL_Surface *pixels=SDL_RenderReadPixels(g_state.renderer,NULL);
        fixture_assert(pixels!=NULL);
        fixture_assert(IMG_SavePNG(pixels,"options-tutorial-portrait.png"));
        SDL_DestroySurface(pixels);
    }
    /* Before the fix the native renderer returns without drawing the card,
     * while the real event owner still consumes every tap. */
    fixture_assert(drawn_count>0);
    fixture_assert(strstr(drawn_content,"Settings")!=NULL);
    fixture_assert(tutorial_buttons[0].w>0);
    fixture_assert(tutorial_buttons[0].y+tutorial_buttons[0].h<=height);
    int before=continued;
    tap(tutorial_buttons[0]);
    fixture_assert(continued==before+1 && !fixture.active);
    fixture_assert(!ui_menu_click_has_pending());
    frame();
    fixture_assert(drawn_count==0);
    for (int choice=1;choice<=9;++choice) {
        menu(9);
        fixture_assert(sdl_character_sheet_screen_commit_select(choice));
        frame();
        tap(hit_rect(choice));
        expect_click(choice);
    }
    for (int choice=-2;choice<=-1;++choice) {
        menu(9);
        tap(hit_rect(choice));
        expect_click(choice);
    }
    /* Overflowing settings lists retain scrolling without activating rows. */
    menu(40);
    fixture_assert(g_sdl_character_sheet_screen.sheet_scroll_max>0);
    SDL_FRect first=hit_rect(1);
    float x=first.x+first.w/2, y=first.y+first.h/2;
    touch(SDL_EVENT_FINGER_DOWN,x,y);
    touch(SDL_EVENT_FINGER_MOTION,x,y-50);
    touch(SDL_EVENT_FINGER_UP,x,y-50);
    fixture_assert(g_sdl_character_sheet_screen.sheet_scroll>0);
    fixture_assert(!ui_menu_click_has_pending());
    sdl_character_sheet_screen_hide(); ui_menu_click_clear();
    check_allocation_gestures();
    sdl_story_font_cache_clear(); sdl_ui_text_cache_clear();
    term_nuke(&view->t); term_screen=NULL; Term=NULL;
    SDL_DestroyRenderer(g_state.renderer); g_state.renderer=NULL;
    SDL_DestroyWindow(g_state.window); g_state.window=NULL;
    printf("Native Options %dx%d: tutorial visible, Continue, all rows, Back, Choose, scroll: PASS\n",
        width,height);
    printf("Native skill allocation %dx%d: long-tap/right-click refund, stale parent press, parent Back restored: PASS\n",
        width,height);
}

int main(int argc,char **argv)
{
    maxima limits={0}; player_type player={0};
    fixture_assert(argc==3);
    setbuf(stdout,NULL); log_set_quiet(true);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");
    fixture_assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS));
    fixture_assert(TTF_Init());
    sdl_config_set_defaults(&config);
    SDL_strlcpy(config.story_font,argv[1],sizeof(config.story_font));
    SDL_strlcpy(config.story_font2,argv[1],sizeof(config.story_font2));
    config.input_ui_mode=SDL_INPUT_UI_MODE_PLATFORM; config.use_unsafe_area=true;
    g_state.system_scale=1; z_info=&limits; p_ptr=&player;
    player.playing=true; character_generated=character_dungeon=true;
    g_state.palette[TERM_L_BLUE]=(SDL_Color){0,210,220,255};
    g_state.palette[TERM_WHITE]=(SDL_Color){200,200,200,255};
    g_state.palette[TERM_YELLOW]=(SDL_Color){255,255,0,255};
    check(580,1280,argv[2]);
    check(360,800,argv[2]);
    check(800,360,argv[2]);
    check(1280,720,argv[2]);
    TTF_Quit(); SDL_Quit();
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/sdl/ui/sdl-gameplay-tutorial.c.obj",
                "/src/sdl/ui/sdl-screens.c.obj")
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects
                                 if not obj.endswith(excluded)), encoding="utf-8")
    env = os.environ.copy()
    # Load the pinned SDL DLLs that match this build's import libraries.
    env["PATH"] = os.pathsep.join(
        [str(BUILD / "_deps" / dep) for dep in ["SDL", "SDL_ttf", "SDL_image", "SDL_mixer"]] +
        ["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    wraps = ["tutorial_get_view", "tutorial_is_active", "tutorial_revision",
             "get_sdl_gameplay_tutorial_mode", "tutorial_continue", "SDL_WaitEvent",
             "sdl_touch_round_layer_controls_active", "sdl_touch_round_compute_layout",
             "sdl_touch_thumb_current_bounds", "sdl_map_grid_cell_rect",
             "sdl_touch_only_device_active", "sdl_get_layout_screen_rect"]
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
                    "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-Wl," + ",".join("--wrap=" + w for w in wraps),
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    catalogue = json.loads((ROOT / "lib/help/tutorials.json").read_text(encoding="utf-8"))
    lesson = next(lesson for lesson in catalogue["lessons"] if lesson["id"] == "menu.settings")
    subprocess.run([str(exe), str(ROOT / "lib/xtra/font/EBGaramond-Regular.ttf"),
                    lesson["steps"][0]["text"]], cwd=OUT, env=env, check=True, timeout=60)


if __name__ == "__main__":
    main()

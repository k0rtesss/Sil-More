#!/usr/bin/env python3
"""Replay real character-sheet Abilities handoffs with isolated SDL/player data.

Checks mouse hover/click wake ordering, held touches, keyboard and controller
entry, navigation, and deliberate Back in normal and larger text modes.
--baseline compiles the committed SDL screen source without changing the tree.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile

from qa9_sdl_fixture import HARNESS

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/character-sheet-abilities"

CHECKS = r'''
#include "sdl/ui/sdl-screens.c"
static int phase, entry_kind, ability_waits;
static bool sent_ability_back;
static SDL_Event opening_release;

static void key_event(SDL_Keycode key, SDL_Scancode scancode)
{
    SDL_Event event={0};
    event.type=SDL_EVENT_KEY_DOWN;
    event.key.windowID=SDL_GetWindowID(g_state.window);
    event.key.timestamp=SDL_GetTicksNS();
    event.key.down=true;event.key.key=key;event.key.scancode=scancode;
    sdl_handle_event(&g_state,&event);
    event.type=SDL_EVENT_KEY_UP;event.key.down=false;
    sdl_handle_event(&g_state,&event);
}

static SDL_Event pointer_event(SDL_EventType type, SDL_FRect rect)
{
    SDL_Event event={0};
    event.type=type;
    if(type==SDL_EVENT_FINGER_DOWN || type==SDL_EVENT_FINGER_UP) {
        int width=0,height=0;
        assert(SDL_GetWindowSizeInPixels(g_state.window,&width,&height));
        event.tfinger.windowID=SDL_GetWindowID(g_state.window);
        event.tfinger.timestamp=SDL_GetTicksNS();
        event.tfinger.touchID=456;event.tfinger.fingerID=123;
        event.tfinger.x=(rect.x+rect.w/2)/width;
        event.tfinger.y=(rect.y+rect.h/2)/height;
    } else {
        event.button.windowID=SDL_GetWindowID(g_state.window);
        event.button.timestamp=SDL_GetTicksNS();
        event.button.button=SDL_BUTTON_LEFT;
        event.button.down=type==SDL_EVENT_MOUSE_BUTTON_DOWN;
        event.button.x=rect.x+rect.w/2;event.button.y=rect.y+rect.h/2;
    }
    return event;
}

static void enter_abilities(void)
{
    assert(g_sdl_character_sheet_screen.context==SDL_CHARACTER_SHEET_LIVE);
    assert(sdl_render_current_window_frame());
    if(entry_kind<3) {
        SDL_FRect rect={0};
        for(int i=0;i<g_sdl_character_sheet_screen.hit_count;++i)
            if(g_sdl_character_sheet_screen.hits[i].choice=='x')
                rect=g_sdl_character_sheet_screen.hits[i].rect;
        assert(rect.w>0 && rect.h>0);
        if(entry_kind==0) {
            /* An animation/flush can dispatch hover and click together before
             * the menu consumes the hover wake. There must be only one wake. */
            SDL_Event motion={0};motion.type=SDL_EVENT_MOUSE_MOTION;
            motion.motion.windowID=SDL_GetWindowID(g_state.window);
            motion.motion.x=rect.x+rect.w/2;motion.motion.y=rect.y+rect.h/2;
            motion.motion.xrel=1;
            sdl_handle_event(&g_state,&motion);
        }
        SDL_Event down=pointer_event(entry_kind==2
            ?SDL_EVENT_FINGER_DOWN:SDL_EVENT_MOUSE_BUTTON_DOWN,rect);
        opening_release=pointer_event(entry_kind==2
            ?SDL_EVENT_FINGER_UP:SDL_EVENT_MOUSE_BUTTON_UP,rect);
        sdl_handle_event(&g_state,&down);
        if(entry_kind==0) sdl_handle_event(&g_state,&opening_release);
    } else if(entry_kind<5) {
        key_event(entry_kind==3?SDLK_X:SDLK_Y,
            entry_kind==3?SDL_SCANCODE_X:SDL_SCANCODE_Y);
    } else {
        SDL_Event event={0};event.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN;
        event.gbutton.timestamp=SDL_GetTicksNS();
        event.gbutton.button=SDL_GAMEPAD_BUTTON_WEST;event.gbutton.down=true;
        sdl_handle_event(&g_state,&event);
        event.type=SDL_EVENT_GAMEPAD_BUTTON_UP;event.gbutton.down=false;
        sdl_handle_event(&g_state,&event);
    }
    phase=1;
}

char __real_inkey(void);
char __wrap_inkey(void)
{
    if(phase==0) {
        /* Inject after the same deferred flush that precedes a real wait. */
        if(inkey_xtra) {Term_flush();inkey_xtra=false;}
        enter_abilities();
    } else if(sdl_terminal_menu_font()==SDL_MENU_FONT_ABILITIES) {
        char key=0;
        ++ability_waits;
        /* Opening a browser must not buy/toggle its initial ability or close
         * it before the player supplies a new command. */
        assert(Term_inkey(&key,false,false)!=0);
        if(ability_waits==1 && entry_kind<3) {
            assert(!sdl_screen_back_gesture_flush_pending_press(
                SDL_GetTicksNS()+1000000000ULL));
            if(entry_kind!=0) sdl_handle_event(&g_state,&opening_release);
            assert(Term_inkey(&key,false,false)!=0);
        }
        assert(ability_waits<=2);
        if(ability_waits==1) {
            Term_keypress('2'); /* Browse another ability without activating it. */
            phase=2;
        } else {
            sent_ability_back=true;
            if(entry_kind==2) {
                /* A fresh long press in the child still performs Back. */
                SDL_Event down=pointer_event(SDL_EVENT_FINGER_DOWN,
                    (SDL_FRect){20,20,2,2});
                assert(!sdl_screen_back_gesture_handle_event(&down));
                assert(sdl_screen_back_gesture_flush_pending_press(
                    SDL_GetTicksNS()+1000000000ULL));
                down.type=SDL_EVENT_FINGER_UP;
                assert(sdl_screen_back_gesture_handle_event(&down));
            } else Term_keypress(ESCAPE);
            phase=3;
        }
    } else {
        assert(sent_ability_back && ability_waits==2);
        assert(g_sdl_character_sheet_screen.context==SDL_CHARACTER_SHEET_LIVE);
        Term_keypress(ESCAPE);
        phase=4;
    }
    return __real_inkey();
}

static void checks(void)
{
    set_sdl_gameplay_tutorial_mode(TUTORIAL_MODE_DISABLED);
    sdl_question_menu_clear();clear_active_narrative_banner();
    sdl_keyboard_capture_cancel();sdl_poetry_screen_hide();sdl_pause_text_screen_hide();
    sdl_tale_screen_hide();sdl_character_sheet_screen_hide();
    memset(p_ptr,0,sizeof(*p_ptr));
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    p_ptr->playing=true;p_ptr->chp=p_ptr->mhp=100;
    p_ptr->py=p_ptr->px=10;p_ptr->cur_map_hgt=p_ptr->cur_map_wid=30;
    p_ptr->song1=p_ptr->song2=SNG_NOTHING;
    p_ptr->new_exp=p_ptr->exp=2000;
    rp_ptr=&p_info[0];current_character_profile=&c_info[0];
    mon_max=o_max=1;o_cnt=0;
    for(int y=0;y<30;++y)for(int x=0;x<30;++x) {
        cave_feat[y][x]=FEAT_FLOOR;cave_info[y][x]=CAVE_VIEW|CAVE_SEEN;
        cave_m_idx[y][x]=cave_o_idx[y][x]=0;
    }
    cave_m_idx[10][10]=-1;
    character_generated=true;character_dungeon=true;
    config.mouse_enabled=true;config.gamepad_enabled=true;
    turn=331;playerturn=33;
    assert(SDL_SetWindowSize(g_state.window,1280,720));
    assert(SDL_SyncWindow(g_state.window));
    entry_kind=atoi(SDL_getenv("ABILITIES_ENTRY"));
    int bigger=atoi(SDL_getenv("ABILITIES_BIG"));
    config.bigger_font=bigger;
    for(int i=0;i<SDL_MENU_FONT_COUNT;++i)config.menu_bigger_font[i]=bigger;
    sdl_apply_config_no_redraw();
    Term_flush();inkey_next_set(NULL);
    phase=ability_waits=0;sent_ability_back=false;
    do_cmd_character_sheet();
    assert(phase==4 && ability_waits==2 && sent_ability_back);
    assert(character_icky==0 && !sdl_screen_back_gesture_active());
    assert(sdl_terminal_menu_font()==SDL_MENU_FONT_NONE);
    assert(!sdl_character_sheet_screen_active() && !ui_menu_click_is_active());
    assert(turn==331 && playerturn==33 && p_ptr->energy_use==0);
    assert(p_ptr->new_exp==2000 && p_ptr->exp==2000);
    printf("Abilities handoff entry=%d bigger=%d: opened, navigated, deliberate Back PASS\n",
        entry_kind,bigger);fflush(stdout);
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", action="store_true")
    parser.add_argument("--case", choices=("all", "mouse", "touch", "keyboard"), default="all")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    checks = CHECKS
    if args.baseline:
        baseline = OUT / "sdl-screens-baseline.c"
        baseline.write_text(subprocess.check_output(
            ["git", "show", "HEAD:src/sdl/ui/sdl-screens.c"], cwd=ROOT,
            text=True, encoding="utf-8"), encoding="utf-8")
        checks = checks.replace('"sdl/ui/sdl-screens.c"', '"' + baseline.as_posix() + '"')
    prefix = HARNESS[:HARNESS.index("static cptr make_path")]
    init = HARNESS[HARNESS.index("static cptr make_path"):HARNESS.index("    int last = ")]
    source = OUT / "check.c"
    source.write_text(prefix + checks + init +
                      "    checks();sdl_quit_hook(NULL);return 0;\n}\n", encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects
        if not obj.endswith(("/src/main.c.obj", "/src/sdl/ui/sdl-screens.c.obj"))), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / dep) for dep in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    env.update(SDL_VIDEO_DRIVER="dummy", SDL_RENDER_DRIVER="software", SDL_AUDIO_DRIVER="dummy",
               CONTROL_TEST_ASSETS=str(ROOT / "lib"))
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-Wl,--wrap=inkey", "-o", str(exe)],
        cwd=BUILD, env=env, check=True)
    entries = {"all": range(6), "mouse": (0, 1), "touch": (2,), "keyboard": (3, 4)}[args.case]
    for bigger in (0, 1):
        for entry in entries:
            with tempfile.TemporaryDirectory(prefix="fixture-", dir=OUT) as profile:
                subprocess.run([str(exe), "--windowed", "--tiles"], cwd=ROOT,
                    env=dict(env, CONTROL_TEST_PROFILE=profile, ABILITIES_ENTRY=str(entry),
                             ABILITIES_BIG=str(bigger)), check=True, timeout=30)


if __name__ == "__main__":
    main()

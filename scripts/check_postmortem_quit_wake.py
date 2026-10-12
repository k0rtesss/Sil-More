#!/usr/bin/env python3
"""Exercise native menu transition wakes through the real command consumer.

Only the menu action boundary is injected. Production overlay choice routing,
the terminal queue, and request_command() decide what the next owner receives.
--baseline restores the duplicate paid-action wake on Quit/Blitz.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/postmortem-quit-wake"

HARNESS = r'''
#include "angband.h"
#include <assert.h>
bool fixture_execute(int choice);
bool fixture_spectator(void);
bool fixture_quit(void);
bool fixture_blitz(void);
#define do_cmd_main_menu_execute_choice fixture_execute
#define death_spectator_active fixture_spectator
#define sdl_quit_transition_active fixture_quit
#define blitz_launch_requested fixture_blitz
/* MENU_SOURCE */
#undef do_cmd_main_menu_execute_choice
#undef death_spectator_active
#undef sdl_quit_transition_active
#undef blitz_launch_requested
static bool spectator,quit_transition,blitz,accepted=true,starts_work,starts_leaving;
static int paid_energy,executions;
bool fixture_spectator(void) {return spectator;}
bool fixture_quit(void) {return quit_transition;}
bool fixture_blitz(void) {return blitz;}
bool fixture_execute(int choice)
{
    (void)choice;executions++;
    if(accepted) {
        p_ptr->energy_use+=paid_energy;
        if(starts_work)p_ptr->smithing=true;
        if(starts_leaving)p_ptr->leaving=true;
        inkey_flag=false; /* A nested modal consumed its own input. */
    }
    return accepted;
}
static errr terminal_extra(int action,int value)
{
    /* The fixture supplies every key; an extra blocking wait is a failure. */
    assert(action!=TERM_XTRA_EVENT || !value);return 0;
}
static void reset(void)
{
    Term_flush();memset(p_ptr,0,sizeof(*p_ptr));
    p_ptr->playing=true;p_ptr->py=p_ptr->px=2;p_ptr->cur_map_hgt=p_ptr->cur_map_wid=20;
    spectator=quit_transition=blitz=starts_work=starts_leaving=false;
    accepted=true;paid_energy=executions=0;inkey_flag=true;character_icky=0;
    movement_input_clear_commands();
}
static void empty(void)
{
    char key=0;int result=Term_inkey(&key,false,true);
    if(!result)fprintf(stderr,"Unexpected leftover key %u\n",(unsigned char)key);
    assert(result!=0);
}
static void consume_transition(int choice,int inherited,bool is_spectator)
{
    reset();p_ptr->energy_use=inherited;spectator=is_spectator;
    quit_transition=choice==MAIN_MENU_SAVE_QUIT&&!is_spectator;
    blitz=choice==MAIN_MENU_BLITZ;
    sdl_main_menu_overlay_choose(choice);assert(executions==1&&inkey_flag);
    request_command();
    if(p_ptr->command_cmd!=ESCAPE)
        fprintf(stderr,"Transition inherited energy %d returned command %d, expected Escape\n",
            inherited,(unsigned char)p_ptr->command_cmd);
    assert(p_ptr->command_cmd==ESCAPE);
    /* Final Look checks its exit flag here. The subsequent initial_menu()
     * must wait for fresh input rather than receiving another Escape. */
    empty();assert(p_ptr->energy_use==inherited);
}
int main(void)
{
    term t;player_type player={0};player_other options={0};maxima limits={0};
    p_ptr=&player;op_ptr=&options;z_info=&limits;setbuf(stdout,NULL);
    assert(SDL_Init(SDL_INIT_EVENTS));sdl_config_set_defaults(&config);
    config.gamepad_enabled=true;config.input_ui_mode=SDL_INPUT_UI_MODE_CONTROLLER;
    assert(!term_init(&t,80,24,256));t.xtra_hook=terminal_extra;
    Term_activate(&t);term_screen=&t;angband_term[0]=&t;log_set_quiet(true);
    /* A fatal paid action is the actual reproduction: inherited energy=100. */
    consume_transition(MAIN_MENU_SAVE_QUIT,100,true);
    consume_transition(MAIN_MENU_SAVE_QUIT,0,true);
    consume_transition(MAIN_MENU_SAVE_QUIT,100,false);
    consume_transition(MAIN_MENU_BLITZ,100,false);
    consume_transition(MAIN_MENU_BLITZ,0,false);
    puts("Postmortem/normal Quit and Blitz: one consumed transition wake, no title-menu Escape replay PASS");
    reset();paid_energy=100;Term_keypress('a');
    sdl_main_menu_overlay_choose(MAIN_MENU_OPTIONS);request_command();
    assert(p_ptr->command_cmd==' '&&p_ptr->energy_use==100);
    char key=0;assert(!Term_inkey(&key,false,true)&&key=='a');empty();
    reset();starts_work=true;
    sdl_main_menu_overlay_choose(MAIN_MENU_SMITHING);request_command();
    assert(p_ptr->command_cmd==' '&&p_ptr->smithing);empty();
    reset();starts_leaving=true;
    sdl_main_menu_overlay_choose(MAIN_MENU_OPTIONS);request_command();
    assert(p_ptr->command_cmd==0&&p_ptr->leaving);empty();
    reset();accepted=false;p_ptr->energy_use=100;
    sdl_main_menu_overlay_choose(MAIN_MENU_OPTIONS);empty();
    puts("Paid modal action/work/leaving still wake once; queued typeahead and refused actions preserved PASS");
    term_nuke(&t);SDL_Quit();return 0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", action="store_true")
    args = parser.parse_args()
    out = OUT / "baseline" if args.baseline else OUT
    out.mkdir(parents=True, exist_ok=True)
    production = (ROOT / "src/sdl/ui/sdl-main-menu.c").read_text()
    if args.baseline:
        current = """if (executed && choice != MAIN_MENU_SAVE_QUIT && choice != MAIN_MENU_BLITZ
        && (p_ptr->energy_use > 0
            || (choice == MAIN_MENU_SMITHING && p_ptr->smithing)
            || (!leaving_before_action && p_ptr->leaving)))"""
        previous = """if (executed && (p_ptr->energy_use > 0
            || (choice == MAIN_MENU_SMITHING && p_ptr->smithing)
            || (!leaving_before_action && p_ptr->leaving
                && choice != MAIN_MENU_SAVE_QUIT && choice != MAIN_MENU_BLITZ)))"""
        assert current in production
        production = production.replace(current, previous, 1)
    source = out / "check.c"
    source.write_text(HARNESS.replace("/* MENU_SOURCE */", production), encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    response = out / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects if not p.endswith(
        ("/src/main.c.obj", "/src/sdl/ui/sdl-main-menu.c.obj"))), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = out / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-o", str(exe)], cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe)], cwd=ROOT, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()

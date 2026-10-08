#!/usr/bin/env python3
"""Check native-menu paid actions wake the blocked engine command request.

Exercises real Crown cutting, its confirmation cancellation, a half action,
free browsing, smithing and real projection-animation typeahead.
--baseline uses committed menu/command sources; --priority-baseline isolates
only the appended-wake ordering defect; --exit-baseline removes only the free-exit wake.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile
from check_game_control import HARNESS

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/qa9-menu-action-energy"
CHECKS = r'''
#include "support/movement-input.h"
extern void process_command(void);
static bool fixture_animation;
static int animation_input, animation_delays, animation_queued_delays;
static int light_gem_index;
static SDL_Event animation_event;
static errr (*production_xtra)(int,int);
extern void __real_handle_stuff(void);
extern void __real_msg_print(cptr);
void __wrap_handle_stuff(void) { if(!fixture_animation) __real_handle_stuff(); }
void __wrap_msg_print(cptr text) { if(!fixture_animation) __real_msg_print(text); }
static errr animation_xtra(int action,int value)
{
    if(fixture_animation && action==TERM_XTRA_DELAY && animation_delays==1) {
        if(animation_input==0) {
            animation_event.key.timestamp=SDL_GetTicksNS();
            assert(SDL_PushEvent(&animation_event));
            SDL_Event release=animation_event;
            release.type=SDL_EVENT_KEY_UP;release.key.down=false;
            assert(SDL_PushEvent(&release));
        } else {
            /* Production movement input carries a semantic command and wake. */
            movement_input_command command={MOVEMENT_INPUT_CONTEXT_DUNGEON,
                MOVEMENT_INPUT_ACTION_MOVE_DIR,MOVEMENT_INPUT_DIRECTION_EAST,0};
            assert(movement_input_submit_command(&command));
            Term_keypress(UI_MENU_CLICK_WAKE_KEY);
        }
    }
    errr result=production_xtra(action,value);
    if(fixture_animation && action==TERM_XTRA_DELAY) {
        char first=0;
        ++animation_delays;
        if(Term_inkey(&first,false,false)==0) ++animation_queued_delays;
    }
    return result;
}
static void animated_gem_action(void)
{
    SDL_Event event={0};
    event.type=SDL_EVENT_KEY_DOWN;
    event.key.windowID=SDL_GetWindowID(g_state.window);
    event.key.timestamp=SDL_GetTicksNS();
    event.key.down=true;
    event.key.key=SDLK_Q;event.key.scancode=SDL_SCANCODE_Q;
    event.key.mod=SDL_KMOD_CTRL;
    animation_event=event;
    /* Same dungeon/command-wait context as a native Inventory Use callback. */
    character_dungeon=true;
    ++character_icky;
    supplies_begin_action(light_gem_index);
    do_cmd_use_gem(supplies_entry_at(light_gem_index),SUPPLIES_INDEX);
    supplies_end_action();
    --character_icky;
    character_dungeon=false;
}
static int fixture_action;
static bool fixture_accept;
bool __wrap_get_check(cptr prompt) { (void)prompt; return fixture_accept; }
bool fixture_menu_choice(int choice)
{
    assert(choice == MAIN_MENU_INVENTORY || choice == MAIN_MENU_SMITHING || choice == MAIN_MENU_OPTIONS);
    if (fixture_action == 1003) {
        if(fixture_accept) {p_ptr->is_dead=true;p_ptr->playing=false;p_ptr->leaving=true;}
        Term_keypress('Q'); /* Typeahead arrived inside the modal callback. */
        return true;
    }
    if (fixture_action == 1002) { animated_gem_action(); return true; }
    if (fixture_action == 1000) return do_cmd_delete_item_by_index(-1);
    if (fixture_action == -1) p_ptr->smithing=3;
    else p_ptr->energy_use=fixture_action;
    return true;
}
static void select_action(int action)
{
    Term_flush(); inkey_next_set(NULL);
    p_ptr->energy_use=0; p_ptr->smithing=0;
    fixture_action=action;
    inkey_flag=true;
    sdl_main_menu_overlay_choose(action == -1 ? MAIN_MENU_SMITHING : action == 1003 ? MAIN_MENU_OPTIONS : MAIN_MENU_INVENTORY);
}
static void expect_paid_wake(int energy)
{
    char key=0;
    assert(Term_inkey(&key,false,true)==0 && key==UI_MENU_CLICK_WAKE_KEY);
    Term_keypress(key); Term_keypress('Q');
    request_command();
    assert(p_ptr->command_cmd==' ' && p_ptr->energy_use==energy);
    assert(Term_inkey(&key,false,true)==0 && key=='Q');
    process_command();
    assert(p_ptr->energy_use==energy); /* Harmless wake never replaces the paid action. */
}

static void check_animation_typeahead(void)
{
    production_xtra=Term->xtra_hook;
    Term->xtra_hook=animation_xtra;
    /* Prove the existing semantic wake control before the legacy SDL key. */
    const int inputs[]={1,0};
    for(int run=0;run<2;++run) {
        sdl_question_menu_clear();clear_active_narrative_banner();
        sdl_keyboard_capture_cancel();sdl_poetry_screen_hide();sdl_pause_text_screen_hide();
        sdl_tale_screen_hide();sdl_character_sheet_screen_hide();
        Term_flush();inkey_next_set(NULL);movement_input_clear_commands();
        memset(p_ptr,0,sizeof(*p_ptr));
        memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
        player_carried_extra_reset_store();player_quiver_reset_store();supplies_reset_store();
        p_ptr->playing=true;p_ptr->chp=p_ptr->mhp=100;
        p_ptr->py=p_ptr->px=10;p_ptr->cur_map_hgt=p_ptr->cur_map_wid=30;
        p_ptr->song1=p_ptr->song2=SNG_NOTHING;
        rp_ptr=&p_info[0];current_character_profile=&c_info[0];
        mon_max=o_max=1;o_cnt=0;
        for(int y=0;y<30;++y)for(int x=0;x<30;++x) {
            cave_feat[y][x]=FEAT_FLOOR;cave_info[y][x]=CAVE_VIEW|CAVE_SEEN;
            cave_m_idx[y][x]=cave_o_idx[y][x]=0;
        }
        cave_m_idx[10][10]=-1;
        object_type gem;
        object_prep(&gem,lookup_kind(TV_GEM,SV_GEM_LIGHT));gem.number=2;
        object_aware(&gem);assert(supplies_absorb_object(&gem));
        light_gem_index=supplies_first_entry_for_kind(lookup_kind(TV_GEM,SV_GEM_LIGHT));
        assert(light_gem_index>=0 && supplies_entry_units(light_gem_index)==2);
        op_ptr->delay_factor=10;msg_flag=false;
        animation_input=inputs[run];animation_delays=animation_queued_delays=0;
        fixture_animation=true;fixture_action=1002;inkey_flag=true;
        movement_input_set_active_context(MOVEMENT_INPUT_CONTEXT_DUNGEON);
        sdl_main_menu_overlay_choose(MAIN_MENU_INVENTORY);
        assert(animation_delays>0 && animation_queued_delays>0);
        assert(p_ptr->energy_use==100 && supplies_entry_units(light_gem_index)==1);
        request_command();
        assert(p_ptr->command_cmd==' ' && p_ptr->energy_use==100);
        process_command();assert(p_ptr->energy_use==100 && p_ptr->px==10 && p_ptr->py==10);
        int paid_energy=p_ptr->energy_use;
        p_ptr->energy_use=0; /* Engine paid the accepted item action. */
        request_command();
        if(animation_input==0) assert(p_ptr->command_cmd==KTRL('Q'));
        else {
            assert(p_ptr->command_cmd==';' && p_ptr->command_dir==6);
            process_command();assert(p_ptr->energy_use==100);
            paid_energy+=p_ptr->energy_use;assert(paid_energy==200);
            assert(p_ptr->px==11 && p_ptr->py==10);
        }
        char next=0;
        assert(Term_inkey(&next,false,true)!=0); /* Typeahead consumed once. */
        fixture_animation=false;
        if(animation_input==1)
            puts("Production semantic movement wake retains a separate paid action and executes once PASS.");
    }
    Term->xtra_hook=production_xtra;
    puts("Real Gem Light projection/SDL Ctrl+Q typeahead paid first; semantic movement retains a separate payment and executes once PASS.");
}

static void check_free_exit(void)
{
    char key=0;turn=991;playerturn=99;p_ptr->energy=73;
    p_ptr->leaving=p_ptr->is_dead=false;p_ptr->playing=true;
    fixture_accept=false;select_action(1003);
    assert(p_ptr->playing&&!p_ptr->is_dead&&!p_ptr->leaving&&!p_ptr->energy_use);
    assert(Term_inkey(&key,false,true)==0&&key=='Q');
    assert(Term_inkey(&key,false,true)!=0); /* Cancelled No has no wake. */
    fixture_accept=true;select_action(1003);
    assert(!p_ptr->playing&&p_ptr->is_dead&&p_ptr->leaving&&!p_ptr->energy_use);
    assert(Term_inkey(&key,false,false)==0&&key==UI_MENU_CLICK_WAKE_KEY);
    request_command();
    assert(p_ptr->command_cmd==0&&!p_ptr->energy_use&&p_ptr->energy==73);
    assert(turn==991&&playerturn==99);
    assert(Term_inkey(&key,false,true)==0&&key=='Q');
    assert(Term_inkey(&key,false,true)!=0); /* Exit did not consume typeahead. */
    select_action(1003); /* Already leaving is not another state transition. */
    assert(Term_inkey(&key,false,true)==0&&key=='Q');
    assert(Term_inkey(&key,false,true)!=0);
    assert(turn==991&&playerturn==99&&!p_ptr->energy_use&&p_ptr->energy==73);
    puts("Native Options free exit wakes once with command0, preserves energy/clocks/typeahead; No and preexisting leaving stay free PASS.");
}

static void checks(void)
{
    p_ptr->playing=true; p_ptr->is_dead=false;
    p_ptr->py=p_ptr->px=10;
    p_ptr->cur_map_hgt=p_ptr->cur_map_wid=30;
    p_ptr->depth=MORGOTH_DEPTH;
    p_ptr->chp=p_ptr->mhp=100;
    mon_max=1; o_max=2;
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    for(int y=0;y<30;y++)for(int x=0;x<30;x++)cave_feat[y][x]=FEAT_FLOOR;
    object_prep(&inventory[INVEN_WIELD],lookup_kind(TV_SWORD,SV_LONG_SWORD));
    object_type* crown=&o_list[1];
    artefact_type* art=&a_info[ART_MORGOTH_3];
    object_prep(crown,lookup_kind(art->tval,art->sval));
    crown->name1=ART_MORGOTH_3;
    crown->iy=crown->ix=10;
    cave_o_idx[10][10]=1;
    /* Guaranteed damage keeps the test about dispatch, not random cut success. */
    p_ptr->mdd=100; p_ptr->mds=100; p_ptr->skill_use[S_MEL]=100;
    turn=331; playerturn=33;
    fixture_accept=false;
    select_action(1000);
    char key=0;
    assert(Term_inkey(&key,false,true)!=0 && !p_ptr->energy_use);
    assert(crown->name1==ART_MORGOTH_3 && !silmarils_possessed());
    fixture_accept=true;
    select_action(1000);
    assert(crown->name1==ART_MORGOTH_2 && silmarils_possessed()==1);
    assert(p_ptr->energy_use==100);
    expect_paid_wake(100);
    assert(turn==331 && playerturn==33 && p_ptr->chp==100);

    select_action(50); expect_paid_wake(50);
    select_action(-1); expect_paid_wake(0); assert(p_ptr->smithing==3);
    select_action(0);
    assert(Term_inkey(&key,false,true)!=0);
    Term_keypress(UI_MENU_CLICK_WAKE_KEY); Term_keypress('Q');
    request_command(); assert(p_ptr->command_cmd=='Q' && !p_ptr->energy_use);
    puts("Native menu real Crown cut/cancel, full/half paid wake, smithing and free browsing PASS.");
    check_animation_typeahead();
    check_free_exit();
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", action="store_true")
    parser.add_argument("--priority-baseline", action="store_true")
    parser.add_argument("--exit-baseline", action="store_true")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    menu = ROOT / "src/sdl/ui/sdl-main-menu.c"
    command = ROOT / "src/support/command.c"
    if args.baseline:
        for relative, name in (("src/sdl/ui/sdl-main-menu.c", "menu-baseline.c"),
                               ("src/support/command.c", "command-baseline.c")):
            path = OUT / name
            path.write_text(subprocess.check_output(["git", "show", "HEAD:" + relative],
                cwd=ROOT, text=True, encoding="utf-8"), encoding="utf-8")
        menu, command = OUT / "menu-baseline.c", OUT / "command-baseline.c"
    if args.priority_baseline:
        assert not args.baseline
        priority = OUT / "menu-priority-baseline.c"
        current = menu.read_text(encoding="utf-8")
        assert "Term_key_push(UI_MENU_CLICK_WAKE_KEY)" in current
        priority.write_text(current.replace("Term_key_push(UI_MENU_CLICK_WAKE_KEY)",
            "Term_keypress(UI_MENU_CLICK_WAKE_KEY)"), encoding="utf-8")
        menu = priority
    if args.exit_baseline:
        assert not args.baseline and not args.priority_baseline
        current=menu.read_text(encoding="utf-8")
        addition="""
            || (!leaving_before_action && p_ptr->leaving
                && choice != MAIN_MENU_SAVE_QUIT && choice != MAIN_MENU_BLITZ)"""
        assert addition in current
        old=OUT/"menu-exit-baseline.c"
        old.write_text(current.replace(addition,""),encoding="utf-8")
        menu=old
    prefix = HARNESS[:HARNESS.index("static cptr make_path")]
    prefix += 'bool fixture_menu_choice(int);\n#define do_cmd_main_menu_execute_choice fixture_menu_choice\n'
    prefix += '#include "' + menu.as_posix() + '"\n#undef do_cmd_main_menu_execute_choice\n'
    init = HARNESS[HARNESS.index("static cptr make_path"):HARNESS.index("    int last = ")]
    source = OUT / "check.c"
    source.write_text(prefix + CHECKS + init + "    checks(); sdl_quit_hook(NULL); return 0;\n}\n", encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects if not p.endswith((
        "/src/main.c.obj", "/src/sdl/ui/sdl-main-menu.c.obj", "/src/support/command.c.obj"))), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / p) for p in
        ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")), "C:/msys64/mingw64/bin",
        "C:/msys64/usr/bin", env["PATH"]])
    env.update(SDL_VIDEO_DRIVER="dummy", SDL_RENDER_DRIVER="software",
        SDL_AUDIO_DRIVER="dummy", CONTROL_TEST_ASSETS=str(ROOT / "lib"))
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), str(command),
        "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
        "-Wl,--wrap=get_check", "-Wl,--wrap=msg_print", "-Wl,--wrap=handle_stuff", "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="fixture-", dir=OUT) as profile:
        subprocess.run([str(exe), "--windowed", "--tiles"], cwd=ROOT,
            env=dict(env, CONTROL_TEST_PROFILE=profile), check=True, timeout=30)


if __name__ == "__main__":
    main()

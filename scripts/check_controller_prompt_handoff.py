#!/usr/bin/env python3
"""Exercise delayed pad navigation across real Horn picker/aim ownership."""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile
from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT/'build-standard'
OUT = ROOT/'scripts/output/controller-prompt-handoff'

CHECKS = r'''
/* GAMEPAD */
/* QUESTION */
/* AIM */
enum { HORN_CONFIRM, HORN_CANCEL, STICK_CANCEL, HORN_START_CANCEL, AIM_COMMIT, LOCATION_CONFIRM, DIRECTION_CONFIRM, KEYBOARD_DIRECTION };
static int scenario, question_reads, aim_reads, effects, bells;
static bool selected_blasting;
void __wrap_message_flush(void) {}
void __wrap_handle_stuff(void) {p_ptr->update=p_ptr->redraw=p_ptr->window=0;}
void __wrap_bell(cptr reason) {(void)reason;bells++;}
bool __wrap_fire_arc(int typ,int dir,int dd,int ds,int dif,int rad,int degrees)
{(void)typ;(void)dir;(void)dd;(void)ds;(void)dif;(void)rad;(void)degrees;effects++;return false;}
void __wrap_monster_perception(bool centered,bool main_roll,int difficulty)
{(void)centered;(void)main_roll;(void)difficulty;}
void __wrap_break_truce(bool obvious) {(void)obvious;}
cptr __wrap_get_sdl_config_path(void) {return "fixture-sdl.json";}
static void button(int id,bool down)
{SDL_GamepadButtonEvent e={0};e.button=id;e.down=down;sdl_gamepad_handle_button(&e);}
static void tap(int id) {button(id,true);button(id,false);}
static void axis(int id,int value)
{SDL_GamepadAxisEvent e={0};e.axis=id;e.value=value;sdl_gamepad_handle_axis(&e);}
static char queued(void)
{char key=0;assert(!Term_inkey(&key,false,true));return key;}
static void empty(void)
{char key=0;assert(Term_inkey(&key,false,true));}
static void quick_down(void)
{
    tap(SDL_GAMEPAD_BUTTON_DPAD_DOWN);
    assert(g_gamepad_state.dpad_pending&&g_gamepad_state.dpad_pending_dir==2);
    assert(!sdl_gamepad_flush_pending_dpad(g_gamepad_state.dpad_pending_time+299000000ULL,false));
    empty();
}
char __wrap_inkey(void)
{
    assert(g_question_menu.active&&question_reads<3);
    if(!question_reads++) {
        assert(g_question_menu.highlight==0&&g_question_menu.count==2);
        if(scenario!=STICK_CANCEL)quick_down();
        else assert(g_gamepad_state.stick_movement[0].pending);
        tap(scenario==HORN_CONFIRM?SDL_GAMEPAD_BUTTON_SOUTH:scenario==HORN_START_CANCEL?SDL_GAMEPAD_BUTTON_START:SDL_GAMEPAD_BUTTON_EAST);
    } else {
        assert(scenario==HORN_CONFIRM&&g_question_menu.highlight==1);
        assert(!g_gamepad_state.dpad_pending);
    }
    return queued();
}
static char next_aim_key(u16b context)
{
    assert(context==(scenario==DIRECTION_CONFIRM||scenario==KEYBOARD_DIRECTION
        ? MOVEMENT_INPUT_CONTEXT_DIRECTION_PROMPT:MOVEMENT_INPUT_CONTEXT_TARGETING));
    assert(aim_reads++<3);
    if(scenario==HORN_CONFIRM) {
        selected_blasting=g_unified_look_prompt.count==5;
        /* Simulate the next input owner's ordinary timer poll. The old
         * delayed Down commits a Force blast here without another pad input. */
        (void)sdl_gamepad_flush_pending_dpad(SDL_GetTicksNS(),true);
        char key=0;if(!Term_inkey(&key,false,true))return key;
        assert(sdl_gamepad_ctrl_active());axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,0);
        tap(SDL_GAMEPAD_BUTTON_EAST);return queued();
    }
    if(scenario==KEYBOARD_DIRECTION)return '6';
    if(aim_reads==1) {
        quick_down();tap(SDL_GAMEPAD_BUTTON_SOUTH);
    }
    return queued();
}
char __wrap_inkey_movement_context(u16b context)
{
    u16b previous=movement_input_active_context();movement_input_set_active_context(context);
    char key=next_aim_key(context);movement_input_set_active_context(previous);return key;
}
static void prepare(void)
{
    memset(p_ptr,0,sizeof(*p_ptr));memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    memset(mon_list,0,MAX_MONSTERS*sizeof(*mon_list));
    player_carried_extra_reset_store();player_quiver_reset_store();supplies_reset_store();player_pack_action_reset();
    for(int y=0;y<20;y++)for(int x=0;x<24;x++) {
        bool border=!y||!x||y==19||x==23;
        cave_feat[y][x]=border?FEAT_WALL_PERM:FEAT_FLOOR;
        cave_info[y][x]=border?CAVE_WALL:CAVE_MARK|CAVE_SEEN|CAVE_GLOW|CAVE_FIRE;
        cave_light[y][x]=2;cave_m_idx[y][x]=cave_o_idx[y][x]=0;
    }
    mon_max=o_max=1;mon_cnt=o_cnt=0;
    p_ptr->py=p_ptr->px=5;p_ptr->cur_map_hgt=20;p_ptr->cur_map_wid=24;p_ptr->depth=1;
    p_ptr->playing=p_ptr->wizard=true;p_ptr->chp=p_ptr->mhp=53;p_ptr->csp=42;p_ptr->msp=59;p_ptr->new_exp=40000;
    cave_m_idx[5][5]=-1;character_generated=character_dungeon=true;character_icky=0;
    inventory[INVEN_LITE].timeout=281;turn=1371;playerturn=137;
    for(int i=0;i<2;i++) {
        object_prep(&inventory[i],lookup_kind(TV_HORN,i?SV_HORN_BLASTING:SV_HORN_FORCE));
        inventory[i].number=1;inventory[i].storage=OBJECT_STORAGE_HARNESS;
        object_aware(&inventory[i]);object_known(&inventory[i]);
    }
    p_ptr->inven_cnt=2;p_ptr->new_exp=40000;
    sdl_config_set_defaults(&config);sdl_config_set_default_gamepad_bindings(&config);
    config.input_ui_mode=SDL_INPUT_UI_MODE_CONTROLLER;config.gamepad_dpad_diagonal_delay_ms=300;
    config.gamepad_use_dpad=config.gamepad_use_left_stick=true;config.gamepad_use_right_stick=false;
    config.gamepad_right_stick_bindings[GAMEPAD_STICK_DIR_RIGHT]=GAMEPAD_BIND_ALT;
    g_gamepad_state.pad_count=1;sdl_gamepad_reset_movement_controls();
    movement_input_clear_commands();movement_input_set_active_context(MOVEMENT_INPUT_CONTEXT_DUNGEON);
    Term_flush();question_reads=aim_reads=effects=bells=0;selected_blasting=false;
    tutorial_set_mode(TUTORIAL_MODE_DISABLED);
}
static void unchanged(void)
{
    assert(turn==1371&&playerturn==137&&p_ptr->energy_use==0);
    assert(p_ptr->py==5&&p_ptr->px==5&&p_ptr->chp==53&&p_ptr->csp==42&&p_ptr->new_exp==40000);
    assert(inventory[INVEN_LITE].timeout==281&&inventory[0].number==1&&inventory[1].number==1);
    assert(!g_gamepad_state.dpad_pending&&!g_gamepad_state.stick_movement[0].pending&&!g_gamepad_state.stick_movement[1].pending);
    empty();
}
static void checks(void)
{
    for(scenario=HORN_CONFIRM;scenario<=HORN_START_CANCEL;scenario++) {
        prepare();
        if(scenario==HORN_CONFIRM)axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,32767);
        if(scenario==STICK_CANCEL) {
            axis(SDL_GAMEPAD_AXIS_RIGHTX,32767);assert(sdl_gamepad_alt_active());
            axis(SDL_GAMEPAD_AXIS_LEFTY,32767);assert(g_gamepad_state.stick_movement[0].pending);
        }
        do_cmd_play_instrument(NULL,0);
        if(scenario==HORN_CONFIRM) {
            if(!selected_blasting||effects||p_ptr->energy_use||p_ptr->csp!=42)
                fprintf(stderr,"Fast picker selectedBlasting%d effects%d energy%d voice%d\n",selected_blasting,effects,p_ptr->energy_use,p_ptr->csp);
            assert(selected_blasting&&question_reads==2&&aim_reads==1&&!effects);
            assert(!sdl_gamepad_ctrl_active());
        } else assert(question_reads==1&&!aim_reads&&!effects);
        movement_input_set_active_context(MOVEMENT_INPUT_CONTEXT_DUNGEON);
        (void)sdl_gamepad_flush_pending_dpad(SDL_GetTicksNS(),true);
        (void)sdl_gamepad_flush_pending_sticks(SDL_GetTicksNS(),true);
        int dir;assert(!movement_input_take_legacy_direction(MOVEMENT_INPUT_CONTEXT_DUNGEON,&dir));
        unchanged();
        if(scenario==STICK_CANCEL) {
            assert(sdl_gamepad_alt_active()); /* Do not retire unrelated role modifiers. */
            axis(SDL_GAMEPAD_AXIS_LEFTY,30000);empty();
            axis(SDL_GAMEPAD_AXIS_RIGHTX,0);assert(!sdl_gamepad_alt_active());
            axis(SDL_GAMEPAD_AXIS_LEFTY,0);axis(SDL_GAMEPAD_AXIS_LEFTY,32767);
            assert(sdl_gamepad_flush_pending_sticks(SDL_GetTicksNS(),true));
            assert(queued()==UI_MENU_CLICK_WAKE_KEY);
            assert(movement_input_take_legacy_direction(MOVEMENT_INPUT_CONTEXT_DUNGEON,&dir)&&dir==2);
            axis(SDL_GAMEPAD_AXIS_LEFTY,0);empty();
        }
    }
    for(scenario=AIM_COMMIT;scenario<=KEYBOARD_DIRECTION;scenario++) {
        prepare();int dir=0,y=0,x=0;
        movement_input_set_active_context(scenario>=DIRECTION_CONFIRM?MOVEMENT_INPUT_CONTEXT_DIRECTION_PROMPT:MOVEMENT_INPUT_CONTEXT_TARGETING);
        if(scenario==AIM_COMMIT) {
            movement_input_set_active_context(MOVEMENT_INPUT_CONTEXT_DUNGEON);axis(SDL_GAMEPAD_AXIS_LEFTY,-32767);
            assert(g_gamepad_state.stick_movement[0].pending);movement_input_set_active_context(MOVEMENT_INPUT_CONTEXT_TARGETING);
            r_info[31].max_num=100;r_info[31].cur_num=0;
            assert(place_monster_one(5,8,31,false,true,NULL));mon_list[cave_m_idx[5][8]].ml=true;
            assert(target_select_aim(10,false,&dir)&&dir==2&&!p_ptr->target_set&&aim_reads==1);
        } else if(scenario==LOCATION_CONFIRM) {
            assert(target_select_location("Select",&y,&x)&&y==6&&x==5&&aim_reads==2);
        } else assert(get_rep_dir(&dir)&&dir==(scenario==DIRECTION_CONFIRM?2:6)&&aim_reads==1);
        unchanged();assert(!bells);if(scenario==AIM_COMMIT)axis(SDL_GAMEPAD_AXIS_LEFTY,0);
    }
    /* Cancel retires only controller navigation, not arbitrary typeahead. */
    prepare();sdl_question_menu_begin("Typeahead");sdl_question_menu_add_entry(0,"a)","Force",TERM_WHITE);
    quick_down();Term_keypress('Z');tap(SDL_GAMEPAD_BUTTON_EAST);
    assert(queued()=='Z'&&queued()==ESCAPE);empty();sdl_question_menu_clear();
    prepare();sdl_question_menu_begin("Confirm typeahead");
    sdl_question_menu_add_entry(0,"a)","Force",TERM_WHITE);sdl_question_menu_add_entry(1,"b)","Blasting",TERM_WHITE);
    quick_down();Term_keypress('Z');tap(SDL_GAMEPAD_BUTTON_SOUTH);
    assert(queued()=='Z'&&queued()==UI_MENU_CLICK_WAKE_KEY&&queued()=='\r');
    assert(sdl_question_menu_take_navigation()==1);empty();sdl_question_menu_clear();
    /* The dungeon still gets one smoothed diagonal, or one delayed cardinal. */
    prepare();button(SDL_GAMEPAD_BUTTON_DPAD_UP,true);button(SDL_GAMEPAD_BUTTON_DPAD_RIGHT,true);
    button(SDL_GAMEPAD_BUTTON_DPAD_UP,false);button(SDL_GAMEPAD_BUTTON_DPAD_RIGHT,false);
    int dir=0;assert(queued()==UI_MENU_CLICK_WAKE_KEY);
    assert(movement_input_take_legacy_direction(MOVEMENT_INPUT_CONTEXT_DUNGEON,&dir)&&dir==9);empty();
    quick_down();assert(sdl_gamepad_flush_pending_dpad(g_gamepad_state.dpad_pending_time+300000000ULL,false));
    assert(queued()==UI_MENU_CLICK_WAKE_KEY&&movement_input_take_legacy_direction(MOVEMENT_INPUT_CONTEXT_DUNGEON,&dir)&&dir==2);empty();
    puts("Controller prompt handoff: real Horn picker/aim cancel free, fast confirm/cancel, committing directions without stray Enter, location/keyboard/typeahead/held sticks/modifiers and dungeon timing PASS");
}
'''


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline',action='store_true',help='Remove only the new confirm/cancel boundary calls')
    args=parser.parse_args();OUT.mkdir(parents=True,exist_ok=True)
    gamepad=(ROOT/'src/sdl/input/sdl-gamepad.c').read_text()
    if args.baseline:
        gamepad=gamepad.replace('        if (sdl_gamepad_settle_modal_navigation(true))\n            return;\n','')
        gamepad=gamepad.replace('        (void)sdl_gamepad_settle_modal_navigation(false);\n','')
    question=(ROOT/'src/ui/question.c').read_text().replace('#include "externs.h"','')
    aim=(ROOT/'src/ui/targeting/aim-select.c').read_text().replace('#include "externs.h"','')
    prefix=ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')].replace('#include "level-generation/level-generation-internal.h"','')
    prefix+=fixture_function('terminal_extra').replace("if (action == TERM_XTRA_EVENT) Term_keypress(' ');",'(void)action;')+'\n'
    init=ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):];init=init[:init.index('    check_templates();')]
    source=OUT/'check.c';source.write_text(prefix+CHECKS.replace('/* GAMEPAD */',gamepad).replace('/* QUESTION */',question).replace('/* AIM */',aim)+init+'assert(!init_flavor_info());flavor_init();checks();SDL_Quit();return 0;\n}\n')
    cmake=BUILD/'CMakeFiles/sil-more.dir';objects=shlex.split((cmake/'objects1.rsp').read_text());response=OUT/'objects.rsp'
    response.write_text('\n'.join('"'+p+'"' for p in objects if not p.endswith(('/src/main.c.obj','/src/sdl/input/sdl-gamepad.c.obj','/src/ui/question.c.obj','/src/ui/targeting/aim-select.c.obj'))))
    env=os.environ.copy();env['PATH']=os.pathsep.join([*(str(BUILD/'_deps'/p) for p in ('SDL','SDL_ttf','SDL_image','SDL_mixer')),'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    wraps=('inkey','inkey_movement_context','message_flush','handle_stuff','bell','fire_arc','monster_perception','break_truce','get_sdl_config_path')
    exe=OUT/'check.exe';subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0','@CMakeFiles/sil-more.dir/includes_C.rsp',str(source),'@'+str(response),'@CMakeFiles/sil-more.dir/linkLibs.rsp',*(f'-Wl,--wrap={n}' for n in wraps),'-o',str(exe)],cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-',dir=OUT) as state:subprocess.run([str(exe),str(ROOT/'lib/edit'),state],cwd=state,env=env,check=True,timeout=20)


if __name__=='__main__':main()

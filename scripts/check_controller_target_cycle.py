#!/usr/bin/env python3
"""Exercise production aim selection with real SDL controller dispatch/Term queue."""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT/'build-standard'
OUT = ROOT/'scripts/output/controller-target-cycle'

CHECKS = r'''
#include "sdl/input/sdl-gamepad.c"
/* AIM_SOURCE */
enum { CYCLE_CONFIRM, CYCLE_CANCEL, MANUAL, ZERO_TARGETS, ONE_TARGET,
       KEYBOARD_CYCLE, LOCATION, NONVERTICAL_FACE, VERTICAL_UP, VERTICAL_DOWN,
       KEYBOARD_VERTICAL_UP, KEYBOARD_VERTICAL_DOWN };
static int scenario, step, bells, foe_count, order_y[3], order_x[3];
static bool vertical;
static int final_y, final_x;
void __wrap_message_flush(void) {}
void __wrap_handle_stuff(void) {p_ptr->update=p_ptr->redraw=p_ptr->window=0;}
void __wrap_bell(cptr reason) {(void)reason;bells++;}
cptr __wrap_get_sdl_config_path(void) {return "fixture-sdl.json";}
static int pad(int id)
{
    char extra=0;assert(Term_inkey(&extra,false,true));
    SDL_GamepadButtonEvent e={0};e.button=id;e.down=true;sdl_gamepad_handle_button(&e);
    e.down=false;sdl_gamepad_handle_button(&e);
    assert(!g_gamepad_button_modifiers[id]);
    char key=0;assert(!Term_inkey(&key,false,true));
    assert(Term_inkey(&extra,false,true));return (unsigned char)key;
}
static void trigger(int value)
{
    SDL_GamepadAxisEvent e={0};e.axis=SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;e.value=value;
    sdl_gamepad_handle_axis(&e);
    char extra=0;assert(Term_inkey(&extra,false,true));
}
static void prompt(bool manual,bool location)
{
    assert(g_unified_look_prompt.active);
    int expected=location?2:3+(vertical?2:0)+(!manual&&foe_count>1&&steamdeck_controls_active()?2:0);
    assert(g_unified_look_prompt.count==expected);
    int previous=0,next=0,up=0,down=0,confirm=0,cancel=0;
    for(int i=0;i<expected;i++) {
        sdl_unified_look_prompt_button_state *button=&g_unified_look_prompt.buttons[i];
        if(button->choice=='-') {
            previous++;
            assert(!strcmp(button->labels[0],"L1 Prev target"));
            assert(!strcmp(button->labels[1],"L1 Prev target"));
            assert(!strcmp(button->labels[2],"L1 Prev"));
            assert(!strcmp(button->labels[3],"L1"));
        }
        if(button->choice=='+') {
            next++;
            assert(!strcmp(button->labels[0],"R1 Next target"));
            assert(!strcmp(button->labels[1],"R1 Next target"));
            assert(!strcmp(button->labels[2],"R1 Next"));
            assert(!strcmp(button->labels[3],"R1"));
        }
        if(button->choice=='u') {
            up++;assert(!strcmp(button->labels[0],steamdeck_controls_active()?"Y Up":"u Up"));
            assert(!strcmp(button->labels[1],button->labels[0])&&!strcmp(button->labels[2],button->labels[0]));
            assert(!strcmp(button->labels[3],steamdeck_controls_active()?"Y":"u"));
        }
        if(button->choice=='d') {
            down++;assert(!strcmp(button->labels[0],steamdeck_controls_active()?"View Down":"d Down"));
            assert(!strcmp(button->labels[1],button->labels[0])&&!strcmp(button->labels[2],button->labels[0]));
            assert(!strcmp(button->labels[3],steamdeck_controls_active()?"View":"d"));
        }
        if(button->choice==(location?'t':'f'))confirm++;
        if(button->choice==ESCAPE)cancel++;
    }
    assert(confirm==1&&cancel==1);
    bool cycle=!location&&!manual&&foe_count>1&&steamdeck_controls_active();
    assert(previous==cycle&&next==cycle);
    assert(up==(vertical&&!location)&&down==(vertical&&!location));
}
char __wrap_inkey_movement_context(u16b context)
{
    assert(context==MOVEMENT_INPUT_CONTEXT_TARGETING&&step<10);
    if(scenario>=VERTICAL_UP&&step)fprintf(stderr,"Vertical control failed to commit scenario%d bells%d\n",scenario,bells);
    if(scenario>=VERTICAL_UP)assert(step==0);
    assert(g_pointer_aim.select_mode&&g_pointer_aim.select_visible);
    if(!step&&scenario!=LOCATION) {
        assert(temp_n==foe_count);
        bool offcardinal=false;
        for(int i=0;i<foe_count;i++) {
            order_y[i]=temp_y[i];order_x[i]=temp_x[i];
            int monster=cave_m_idx[temp_y[i]][temp_x[i]];
            assert(monster>0&&target_able(monster)&&mon_list[monster].r_idx==31);
            int dy=abs(temp_y[i]-p_ptr->py),dx=abs(temp_x[i]-p_ptr->px);
            if(dy&&dx&&dy!=dx)offcardinal=true;
        }
        if(foe_count==3)assert(offcardinal);
    }
    bool manual=scenario==LOCATION||foe_count==0
        ||(scenario==MANUAL&&step>=1&&step<=3);
    assert(g_pointer_aim.select_manual==manual);
    assert(g_pointer_aim.select_location==(scenario==LOCATION));
    prompt(manual,scenario==LOCATION);
    int index=0,y=p_ptr->py,x=p_ptr->px;
    if(scenario==CYCLE_CONFIRM||scenario==CYCLE_CANCEL) {
        const int indices[]={0,1,2,0,2,1,0,1};index=indices[step];
    } else if(scenario==KEYBOARD_CYCLE) {
        const int indices[]={0,1,0,2};index=indices[step];
    }
    if(foe_count&&scenario!=LOCATION){y=order_y[index];x=order_x[index];}
    if(scenario==LOCATION&&step==5)x++;
    if(g_pointer_aim.select_y!=y||g_pointer_aim.select_x!=x)
        fprintf(stderr,"scenario%d step%d expected(%d,%d) actual(%d,%d) bells%d\n",scenario,step,y,x,g_pointer_aim.select_y,g_pointer_aim.select_x,bells);
    assert(g_pointer_aim.select_y==y&&g_pointer_aim.select_x==x);
    final_y=y;final_x=x;
    int s=step++;
    if(scenario==CYCLE_CONFIRM||scenario==CYCLE_CANCEL) {
        if(s<3)return pad(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
        if(s<6)return pad(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
        if(s==6){trigger(32767);assert(sdl_gamepad_ctrl_active());int key=pad(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);trigger(0);assert(!sdl_gamepad_ctrl_active());return key;}
        return pad(scenario==CYCLE_CONFIRM?SDL_GAMEPAD_BUTTON_SOUTH:SDL_GAMEPAD_BUTTON_EAST);
    }
    if(scenario==MANUAL) {
        const int inputs[]={SDL_GAMEPAD_BUTTON_WEST,SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
            SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,SDL_GAMEPAD_BUTTON_WEST,SDL_GAMEPAD_BUTTON_EAST};
        return pad(inputs[s]);
    }
    if(scenario==ZERO_TARGETS||scenario==ONE_TARGET)
        return pad(s==0?SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:s==1?SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:SDL_GAMEPAD_BUTTON_EAST);
    if(scenario==KEYBOARD_CYCLE){const char keys[]={'+','-','-','\r'};return keys[s];}
    if(scenario==LOCATION) {
        if(s<2)return pad(s?SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
        if(s<4)return pad(s==2?SDL_GAMEPAD_BUTTON_NORTH:SDL_GAMEPAD_BUTTON_BACK);
        return s==4?'6':pad(SDL_GAMEPAD_BUTTON_SOUTH);
    }
    if(scenario==NONVERTICAL_FACE)
        return pad(s==0?SDL_GAMEPAD_BUTTON_NORTH:s==1?SDL_GAMEPAD_BUTTON_BACK:SDL_GAMEPAD_BUTTON_EAST);
    if(scenario==KEYBOARD_VERTICAL_UP||scenario==KEYBOARD_VERTICAL_DOWN)
        return scenario==KEYBOARD_VERTICAL_UP?'u':'d';
    return pad(scenario==VERTICAL_UP?SDL_GAMEPAD_BUTTON_NORTH:SDL_GAMEPAD_BUTTON_BACK);
}
static void prepare(int count)
{
    memset(p_ptr,0,sizeof(*p_ptr));memset(mon_list,0,MAX_MONSTERS*sizeof(*mon_list));
    memset(cave_m_idx,0,MAX_DUNGEON_HGT*sizeof(*cave_m_idx));
    memset(cave_o_idx,0,MAX_DUNGEON_HGT*sizeof(*cave_o_idx));
    p_ptr->py=p_ptr->px=5;p_ptr->depth=1;p_ptr->cur_map_hgt=20;p_ptr->cur_map_wid=24;
    p_ptr->playing=p_ptr->wizard=true;hjkl_movement=false;p_ptr->chp=p_ptr->mhp=53;p_ptr->csp=42;p_ptr->new_exp=40000;
    for(int y=0;y<20;y++)for(int x=0;x<24;x++) {
        bool border=!y||!x||y==19||x==23;
        cave_feat[y][x]=border?FEAT_WALL_PERM:FEAT_FLOOR;cave_light[y][x]=2;
        cave_info[y][x]=border?CAVE_WALL:CAVE_MARK|CAVE_GLOW|CAVE_SEEN|CAVE_FIRE;
    }
    cave_m_idx[5][5]=-1;mon_max=1;mon_cnt=0;r_info[31].cur_num=0;r_info[31].max_num=100;
    const int coords[][2]={{5,8},{8,7},{2,4}};
    for(int i=0;i<count;i++) {
        assert(place_monster_one(coords[i][0],coords[i][1],31,false,true,NULL));
        mon_list[cave_m_idx[coords[i][0]][coords[i][1]]].ml=true;
    }
    character_dungeon=character_generated=true;character_icky=0;turn=3911;playerturn=391;
    inventory[INVEN_LITE].timeout=281;foe_count=count;step=bells=0;
    movement_input_clear_commands();movement_input_set_active_context(MOVEMENT_INPUT_CONTEXT_TARGETING);
    sdl_gamepad_reset_movement_controls();Term_flush();
}
static void checks(void)
{
    sdl_config_set_defaults(&config);sdl_config_set_default_gamepad_bindings(&config);
    g_gamepad_state.pad_count=1;
    int default_l1=config.gamepad_button_bindings[SDL_GAMEPAD_BUTTON_LEFT_SHOULDER];
    int default_r1=config.gamepad_button_bindings[SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER];
    int default_y=config.gamepad_button_bindings[SDL_GAMEPAD_BUTTON_NORTH];
    int default_view=config.gamepad_button_bindings[SDL_GAMEPAD_BUTTON_BACK];
    for(int vertical_targets=0;vertical_targets<3;vertical_targets++)
    for(int remapped=0;remapped<2;remapped++)
    for(int keyboard_ui=0;keyboard_ui<2;keyboard_ui++)
    for(int mode=0;mode<2;mode++)for(scenario=CYCLE_CONFIRM;scenario<=KEYBOARD_VERTICAL_DOWN;scenario++) {
        if(keyboard_ui&&scenario!=KEYBOARD_CYCLE&&scenario<KEYBOARD_VERTICAL_UP)continue;
        if(vertical_targets&&(scenario!=VERTICAL_UP&&scenario!=VERTICAL_DOWN))continue;
        if(remapped&&(keyboard_ui||(scenario>CYCLE_CANCEL&&scenario!=VERTICAL_UP&&scenario!=VERTICAL_DOWN)))continue;
        if(scenario==NONVERTICAL_FACE&&mode)continue;
        config.gamepad_button_bindings[SDL_GAMEPAD_BUTTON_LEFT_SHOULDER]=remapped?GAMEPAD_BIND_SHIFT:default_l1;
        config.gamepad_button_bindings[SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER]=remapped?'Q':default_r1;
        config.gamepad_button_bindings[SDL_GAMEPAD_BUTTON_NORTH]=remapped?GAMEPAD_BIND_ALT:default_y;
        config.gamepad_button_bindings[SDL_GAMEPAD_BUTTON_BACK]=remapped?'Q':default_view;
        if(scenario>=VERTICAL_UP&&!mode)continue;
        vertical=mode;config.input_ui_mode=keyboard_ui?SDL_INPUT_UI_MODE_PLATFORM:SDL_INPUT_UI_MODE_CONTROLLER;
        prepare(scenario==ZERO_TARGETS?0:scenario==ONE_TARGET?1:vertical_targets==1?0:vertical_targets==2?1:3);
        int dir=0,y=0,x=0;
        bool accepted=scenario==LOCATION?target_select_location("Select",&y,&x):target_select_aim(10,vertical,&dir);
        bool expected=scenario==CYCLE_CONFIRM||scenario==KEYBOARD_CYCLE||scenario==LOCATION||scenario>=VERTICAL_UP;
        assert(accepted==expected);
        if(scenario==LOCATION)assert(y==5&&x==6&&bells==4&&p_ptr->target_set&&p_ptr->target_who==0);
        else if(scenario==NONVERTICAL_FACE)assert(bells==2);
        else assert(!bells);
        if(accepted&&scenario!=LOCATION) {
            if(scenario==VERTICAL_UP||scenario==KEYBOARD_VERTICAL_UP)assert(dir==DIRECTION_UP);
            else if(scenario==VERTICAL_DOWN||scenario==KEYBOARD_VERTICAL_DOWN)assert(dir==DIRECTION_DOWN);
            else assert(dir==5&&p_ptr->target_set&&p_ptr->target_row==final_y&&p_ptr->target_col==final_x&&p_ptr->target_who>0);
        }
        if(!expected||scenario>=VERTICAL_UP)assert(!p_ptr->target_set);
        assert(!g_pointer_aim.select_mode&&!g_unified_look_prompt.active&&temp_n==0);
        assert(p_ptr->py==5&&p_ptr->px==5&&p_ptr->energy_use==0&&p_ptr->chp==53&&p_ptr->csp==42&&p_ptr->new_exp==40000);
        assert(turn==3911&&playerturn==391&&inventory[INVEN_LITE].timeout==281);
        char extra;assert(Term_inkey(&extra,false,true));
        assert(!sdl_gamepad_shift_active()&&!sdl_gamepad_ctrl_active()&&!sdl_gamepad_alt_active());
        assert(!g_gamepad_state.shoulder_pending&&!g_gamepad_state.dpad_pending&&!g_gamepad_focus_chord_pending);
    }
    puts("Aim real selector/controller queue: 3 foes, target wrap/confirm/cancel, prompt IDs/labels, manual/zero/single/location/vertical/keyboard and free resources PASS");
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', action='store_true', help='Disable only controller shoulder mapping')
    parser.add_argument('--vertical-baseline', action='store_true', help='Disable only physical Y/View vertical mapping')
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    aim = (ROOT/'src/ui/targeting/aim-select.c').read_text().replace('#include "externs.h"','')
    if args.baseline:
        for key, value in (('prev','-'),('next','+')):
            clause = f"            else if (!location_mode && query == steamdeck_{key}_page_key())\n                query = '{value}';\n"
            assert aim.count(clause)==1
            aim = aim.replace(clause,'')
    if args.vertical_baseline:
        for key, value in (('secondary','up_key'),('info',"'d'")):
            clause = f"            else if (allow_vertical && !location_mode\n                && query == steamdeck_{key}_key())\n                query = {value};\n"
            assert aim.count(clause)==1
            aim = aim.replace(clause,'')
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]
    prefix = prefix.replace('#include "level-generation/level-generation-internal.h"','')
    prefix += fixture_function('terminal_extra').replace("if (action == TERM_XTRA_EVENT) Term_keypress(' ');",'(void)action;')+'\n'
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init = init[:init.index('    check_templates();')]
    source = OUT/'check.c'
    source.write_text(prefix+CHECKS.replace('/* AIM_SOURCE */',aim)+init+'checks(); SDL_Quit(); return 0;\n}\n')
    cmake = BUILD/'CMakeFiles/sil-more.dir'
    objects = shlex.split((cmake/'objects1.rsp').read_text())
    response = OUT/'objects.rsp'
    response.write_text('\n'.join('"'+p+'"' for p in objects if not p.endswith(
        ('/src/main.c.obj','/src/sdl/input/sdl-gamepad.c.obj','/src/ui/targeting/aim-select.c.obj'))))
    env = os.environ.copy()
    env['PATH'] = os.pathsep.join([*(str(BUILD/'_deps'/p) for p in ('SDL','SDL_ttf','SDL_image','SDL_mixer')),
        'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    exe = OUT/'check.exe'
    wraps = ('inkey_movement_context','message_flush','handle_stuff','bell','get_sdl_config_path')
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0',
        '@CMakeFiles/sil-more.dir/includes_C.rsp',str(source),'@'+str(response),'@CMakeFiles/sil-more.dir/linkLibs.rsp',
        *(f'-Wl,--wrap={name}' for name in wraps),'-o',str(exe)],cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-',dir=OUT) as state:
        subprocess.run([str(exe),str(ROOT/'lib/edit'),state],cwd=state,env=env,check=True,timeout=20)


if __name__ == '__main__':
    main()

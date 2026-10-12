#!/usr/bin/env python3
"""Run the production log viewer with real SDL controller dispatch and key queue."""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build-standard'
OUT = ROOT / 'scripts/output/log-controller-filters'

CHECKS = r'''
#include "sdl/input/sdl-gamepad.c"
/* VIEWER */
static int step;
static bool bigger, keyboard;
static int fixture_cols;
static char log_header[160], footer[160];
void __wrap_handle_stuff(void) {}
void __wrap_message_flush(void) {}
void __wrap_sdl_push_terminal_menu_scale_for(enum sdl_menu_font menu) {(void)menu;}
void __wrap_sdl_pop_terminal_menu_scale(void) {}
bool __wrap_get_sdl_menu_bigger_font(void) {return bigger;}
static int button(int id)
{
    char before=0;int had=Term_inkey(&before,false,true);if(!had)fprintf(stderr,"pre step%d key%d\n",step,before);assert(had);
    SDL_GamepadButtonEvent e={0};e.button=id;e.down=true;
    sdl_gamepad_handle_button(&e);
    (void)sdl_gamepad_flush_pending_dpad(SDL_GetTicksNS(),true);
    e.down=false;sdl_gamepad_handle_button(&e);
    char key=0;int empty=Term_inkey(&key,false,true);if(empty)fprintf(stderr,"no key step%d\n",step);assert(!empty);
    char extra=0;int more=Term_inkey(&extra,false,true);if(!more)fprintf(stderr,"step%d key%d extra%d\n",step,key,extra);assert(more);return (unsigned char)key;
}
static int stick(int axis,int value)
{
    SDL_GamepadAxisEvent e={0};e.axis=axis;e.value=value;sdl_gamepad_handle_axis(&e);
    (void)sdl_gamepad_flush_pending_sticks(SDL_GetTicksNS(),true);
    e.value=0;sdl_gamepad_handle_axis(&e);
    char key=0;int empty=Term_inkey(&key,false,true);if(empty)fprintf(stderr,"no key step%d\n",step);assert(!empty);
    char extra=0;int more=Term_inkey(&extra,false,true);if(!more)fprintf(stderr,"step%d key%d extra%d\n",step,key,extra);assert(more);return (unsigned char)key;
}
char __wrap_inkey(void)
{
    static const char* filters[]={"All","Log","Combat","Notes","All","Notes","Combat","Log","All"};
    assert(step<22);
    int top,bottom,prompt;log_history_layout(Term->hgt,&top,&bottom,&prompt);
    for(int x=0;x<Term->wid && x<159;x++) {
        byte attr;char glyph;Term_what(x,top>0?top-1:0,&attr,&glyph);log_header[x]=glyph;
        Term_what(x,prompt,&attr,&glyph);footer[x]=glyph;
    }
    log_header[MIN(Term->wid,159)]=footer[MIN(Term->wid,159)]=0;
    if(step<=8) {
        char expected[40];strnfmt(expected,sizeof(expected),"Log (%s,",filters[step]);
        if(strncmp(log_header,expected,strlen(expected)))fprintf(stderr,"step%d expected%s got%s\n",step,expected,log_header);
        assert(!strncmp(log_header,expected,strlen(expected)));
    }
    if(!keyboard){assert(strstr(footer,"filter")&&strstr(footer,"L1/R1 page"));
        if(fixture_cols==30)assert(strstr(footer,"D-pad filter")&&strstr(footer,"B back"));}
    if(step==9)assert(strstr(log_header,"Log (All,")&&!strstr(log_header,"Log (All, 1-"));
    if(step==10||step==12||step==14||step==16)assert(strstr(log_header,"Log (All, 1-"));
    if(step==11||step==13||step==15)assert(strstr(log_header,"Log (All, 2-"));
    int s=step++;
    if(s<4)return keyboard?'i':button(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
    if(s<8)return keyboard?'[':button(SDL_GAMEPAD_BUTTON_DPAD_LEFT);
    if(s==8)return keyboard?'p':button(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    if(s==9)return keyboard?'n':button(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
    if(s==10)return keyboard?'8':button(SDL_GAMEPAD_BUTTON_DPAD_UP);
    if(s==11)return keyboard?'2':button(SDL_GAMEPAD_BUTTON_DPAD_DOWN);
    if(s==12)return keyboard?'8':stick(SDL_GAMEPAD_AXIS_LEFTY,-32767);
    if(s==13)return keyboard?'2':stick(SDL_GAMEPAD_AXIS_LEFTY,32767);
    if(s==14)return keyboard?'8':stick(SDL_GAMEPAD_AXIS_RIGHTY,-32767);
    if(s==15)return keyboard?'2':stick(SDL_GAMEPAD_AXIS_RIGHTY,32767);
    if(s==16) {
        if(keyboard)return '\t';
        SDL_GamepadAxisEvent e={0};e.axis=SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;e.value=32767;
        sdl_gamepad_handle_axis(&e);int key=button(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
        e.value=0;sdl_gamepad_handle_axis(&e);assert(!sdl_gamepad_ctrl_active());return key;
    }
    if(s==17){assert(strstr(log_header,"Log (Log,"));return keyboard?'f':button(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);}
    if(s==18){assert(strstr(log_header,"Log (Combat,"));return keyboard?'e':button(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);}
    if(s==19){assert(strstr(log_header,"Log (Notes,"));return keyboard?']':button(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);}
    assert(s==20 && strstr(log_header,"Log (All,"));
    return keyboard?ESCAPE:button(SDL_GAMEPAD_BUTTON_EAST);
}
static void checks(void)
{
    sdl_config_set_defaults(&config);sdl_config_set_default_gamepad_bindings(&config);
    config.gamepad_use_dpad=config.gamepad_use_left_stick=config.gamepad_use_right_stick=true;
    g_gamepad_state.pad_count=1;sdl_gamepad_reset_movement_controls();
    tutorial_set_mode(TUTORIAL_MODE_DISABLED);
    for(int i=0;i<80;i++)message_add(format("Recorded message %d with a readable body.",i),MSG_GENERIC);
    p_ptr->chp=53;p_ptr->csp=42;p_ptr->new_exp=40000;turn=1611;playerturn=161;
    inventory[INVEN_LITE].timeout=281;
    for(int big=0;big<3;big++)for(int keys=0;keys<2;keys++) {
        bigger=big;keyboard=keys;config.input_ui_mode=keys?SDL_INPUT_UI_MODE_PLATFORM:SDL_INPUT_UI_MODE_CONTROLLER;
        fixture_cols=big==2?30:big?64:96;int rows=big==2?9:big?24:36;
        if(Term->wid!=fixture_cols||Term->hgt!=rows)assert(!Term_resize(fixture_cols,rows));step=0;log_header[0]=footer[0]=0;
        do_cmd_messages_with_filter(LOG_HISTORY_FILTER_ALL);
        assert(step==21&&turn==1611&&playerturn==161&&p_ptr->chp==53&&p_ptr->csp==42&&p_ptr->new_exp==40000);
        assert(inventory[INVEN_LITE].timeout==281);
        char extra;assert(Term_inkey(&extra,false,true));
    }
    puts("Log real viewer: controller filter wrap, shoulders/pages, D-pad/both sticks/held trigger, keyboard filters, B return and free resources PASS");
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', action='store_true')
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    viewer = (ROOT/'src/cmd/ui/cmd-ui-main-menu.c').read_text(encoding='utf-8').replace('#include "externs.h"','')
    if args.baseline:
        viewer = viewer.replace("ch == LOG_HISTORY_PREV_FILTER_KEY || ch == '4'", 'ch == LOG_HISTORY_PREV_FILTER_KEY')
        viewer = viewer.replace("ch == LOG_HISTORY_NEXT_FILTER_KEY || ch == '6'", 'ch == LOG_HISTORY_NEXT_FILTER_KEY')
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]
    prefix = prefix.replace('#include "level-generation/level-generation-internal.h"', '')
    prefix += fixture_function('terminal_extra').replace("if (action == TERM_XTRA_EVENT) Term_keypress(' ');", '(void)action;')+'\n'
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init = init[:init.index('    check_templates();')]
    source = OUT/'check.c'
    source.write_text(prefix+CHECKS.replace('/* VIEWER */',viewer)+init+'    checks();SDL_Quit();return 0;\n}\n',encoding='utf-8')
    cmake = BUILD/'CMakeFiles/sil-more.dir'
    objects = shlex.split((cmake/'objects1.rsp').read_text())
    response = OUT/'objects.rsp'
    response.write_text('\n'.join('"'+p+'"' for p in objects if not p.endswith(
        ('/src/main.c.obj','/src/cmd/ui/cmd-ui-main-menu.c.obj','/src/sdl/input/sdl-gamepad.c.obj'))))
    env = os.environ.copy()
    env['PATH'] = os.pathsep.join([*(str(BUILD/'_deps'/p) for p in ('SDL','SDL_ttf','SDL_image','SDL_mixer')),
        'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    wraps = ('inkey','message_flush','handle_stuff','sdl_push_terminal_menu_scale_for',
             'sdl_pop_terminal_menu_scale','get_sdl_menu_bigger_font')
    exe = OUT/'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0','-g',
        '@CMakeFiles/sil-more.dir/includes_C.rsp',str(source),'@'+str(response),'@CMakeFiles/sil-more.dir/linkLibs.rsp',
        *(f'-Wl,--wrap={name}' for name in wraps),'-o',str(exe)],cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-',dir=OUT) as state:
        subprocess.run([str(exe),str(ROOT/'lib/edit'),state],cwd=state,env=env,check=True,timeout=20)


if __name__ == '__main__':
    main()

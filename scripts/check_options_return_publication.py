#!/usr/bin/env python3
"""Check actual Options cleanup repairs clipped gameplay cells and publishes the result.

Extracts the full production Options function; only menu selection and frontend scale boundaries are wrapped. Uses the real
screen stack and Term resize/load; a redraw boundary models the
engine's repaired map cells, while real terminal hooks track published pixels.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function
OLD_CLEANUP = r'''
    if (p_ptr)
    {
        int width = 0;
        int height = 0;
        bool projection_changed;

        Term_get_size(&width, &height);
        projection_changed = width != original_width
            || height != original_height || use_bigtile != original_tiles
            || get_sdl_mobile_portrait_mode() != original_portrait;
        /* A resized saved screen still contains cells from the old map
         * projection. Rebuild the live map after restoring the final view. */
        if (projection_changed && character_dungeon && p_ptr->playing) {
            p_ptr->update |= PU_PANEL;
            p_ptr->redraw |= PR_MAP;
        }
        handle_stuff();
        if (projection_changed && character_dungeon && p_ptr->playing
            && !character_icky && Term)
        {
            Term_fresh();
        }
    }
'''

CHECKS = r'''
static char canvas[90][180];
static int repairs,handles,expected_width,expected_height,menu_width,menu_height,context;
static bool expect_projection;
static errr publish_text(int x,int y,int n,byte attr,cptr text)
{(void)attr;assert(x>=0&&y>=0&&y<90&&x+n<=180);memcpy(canvas[y]+x,text,n);return 0;}
static errr publish_wipe(int x,int y,int n)
{assert(x>=0&&y>=0&&y<90&&x+n<=180);memset(canvas[y]+x,' ',n);return 0;}
static errr publish_extra(int action,int value)
{(void)value;if(action==TERM_XTRA_CLEAR)memset(canvas,' ',sizeof(canvas));return 0;}
void __wrap_handle_stuff(void)
{
 handles++;
 if(!(p_ptr->redraw&PR_MAP))return;
 assert(context<2&&Term->wid==expected_width&&Term->hgt==expected_height&&character_icky==0);
 assert((p_ptr->redraw&(PR_MAP|PR_BASIC|PR_EXTRA))==(PR_MAP|PR_BASIC|PR_EXTRA));
 assert(!!(p_ptr->update&PU_PANEL)==expect_projection);repairs++;
 Term_putch(expected_width-3,expected_height-3,TERM_WHITE,'R');
 p_ptr->update=p_ptr->redraw=p_ptr->window=0;
}
void __wrap_sdl_push_terminal_menu_scale_for(int font)
{(void)font;if(Term->wid!=menu_width||Term->hgt!=menu_height)assert(!Term_resize(menu_width,menu_height));}
void __wrap_sdl_pop_terminal_menu_scale(void)
{if(Term->wid!=expected_width||Term->hgt!=expected_height)assert(!Term_resize(expected_width,expected_height));}
void __wrap_sdl_music_play_menu_theme(void) {}
void __wrap_sdl_refresh_supporting_panes_layout(void) {}
void __wrap_sdl_suspend_main_view_zoom_for_saved_screen(void) {}
bool __wrap_sdl_resume_main_view_zoom_for_saved_screen(void) {return false;}
void __wrap_sdl_music_stop_main(void) {}
void __wrap_message_flush(void) {}
bool __wrap_dismiss_active_narrative_banner(void) {return false;}
bool __wrap_sdl_touch_settings_tutorial_requested(void) {return false;}
bool __wrap_sdl_mouse_settings_tutorial_requested(void) {return false;}
bool __wrap_sdl_character_wheel_settings_tutorial_requested(void) {return false;}
bool __wrap_sdl_zones_settings_tutorial_requested(void) {return false;}
void __wrap_sdl_zones_show_requested_tutorial(void) {}
void fixture_options(void);
static void checks(void)
{
 const int sizes[][2]={{96,36},{160,56}};
 Term->text_hook=publish_text;Term->wipe_hook=publish_wipe;Term->xtra_hook=publish_extra;
 Term->higher_pict=Term->always_pict=false;
 turn=4361;playerturn=436;p_ptr->chp=48;p_ptr->csp=59;p_ptr->new_exp=47464;p_ptr->energy_use=0;inventory[INVEN_LITE].timeout=159;
 for(int size=0;size<2;size++)for(int direction=0;direction<2;direction++)
 for(int changed=0;changed<2;changed++)for(context=0;context<4;context++) {
  expected_width=sizes[size][0];expected_height=sizes[size][1];expect_projection=changed;
  int original_width=changed?expected_width-12:expected_width;
  int original_height=changed?expected_height-4:expected_height;
  if(Term->wid!=original_width||Term->hgt!=original_height)assert(!Term_resize(original_width,original_height));
  menu_width=direction?expected_width+12:expected_width*2/3;
  menu_height=direction?expected_height+4:expected_height*2/3;
  character_generated=context!=3;character_dungeon=context!=3;p_ptr->playing=context!=1;p_ptr->is_dead=context==1;
  p_ptr->update=p_ptr->redraw=p_ptr->window=0;repairs=handles=0;assert(character_icky==0);
  Term_clear();Term_putch(original_width-3,original_height-3,TERM_WHITE,'.');Term_fresh();
  if(context==2)screen_save();
  fixture_options();
  assert(character_icky==(context==2));
  assert(handles==(context<2?1:0));
  int x=expected_width-3,y=expected_height-3;
  if(context<2){if(!repairs)fprintf(stderr,"Options missing map repair size%d direction%d changed%d context%d\n",size,direction,changed,context);if(repairs!=1||Term->scr->c[y][x]!='R'||canvas[y][x]!='R')fprintf(stderr,"case%d/%d/%d/%d repairs%d scr%d canvas%d\n",size,direction,changed,context,repairs,Term->scr->c[y][x],canvas[y][x]);assert(repairs==1&&Term->scr->c[y][x]=='R'&&canvas[y][x]=='R');}
  else {assert(!repairs&&canvas[y][x]==(Term->scr->c[y][x]?Term->scr->c[y][x]:' '));assert(!p_ptr->update&&!p_ptr->redraw&&!p_ptr->window);}
  if(context==2)screen_load();
  assert(turn==4361&&playerturn==436&&p_ptr->energy_use==0&&p_ptr->chp==48&&p_ptr->csp==59&&p_ptr->new_exp==47464&&inventory[INVEN_LITE].timeout==159);
 }
 puts("Actual Options function real save/load/resize/publication: 32 living/postmortem/nested/prebirth geometry cases and free resources PASS");
}
'''



ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT/'build-standard'
OUT = ROOT/'scripts/output/options-return-publication'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', action='store_true', help='Restore only original Options cleanup')
    parser.add_argument('--no-publication', action='store_true', help='Remove only final publication')
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    viewer=(ROOT/'src/cmd/ui/cmd-ui-settings.c').read_text()
    start=viewer.index('void do_cmd_options(void)')
    end=viewer.index('\n/*',start)
    function=viewer[start:end].replace('void do_cmd_options(void)','void fixture_options(void)')
    cleanup_start=function.index('    if (p_ptr)\n    {',function.index('    /* Load screen */'))
    cleanup_end=function.index('    if (sdl_touch_settings_tutorial_requested()',cleanup_start)
    if args.baseline:
        function=function[:cleanup_start]+OLD_CLEANUP+function[cleanup_end:]
    elif args.no_publication:
        cleanup=function[cleanup_start:cleanup_end]
        assert cleanup.count('    Term_fresh();')==1
        function=function[:cleanup_start]+cleanup.replace('    Term_fresh();','')+function[cleanup_end:]
    cleanup_source=OUT/'options.c'
    stubs="""#include "angband.h"
#include "externs.h"
#include "ui/menu-click.h"
#include "sdl-config.h"
#include "sdl-sound.h"
static int options_menu(int* highlight){(void)highlight;return 10;}
static void settings_semantic_menu_hide(void) {}
static void do_cmd_input_options_submenu(int* highlight){(void)highlight;}
static void options_queue_page_turn(int choice,int* queued){(void)choice;(void)queued;}
static void do_cmd_other_options(void) {}
"""
    cleanup_source.write_text(stubs+function)
    checks = CHECKS
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]+fixture_function('terminal_extra')+'\n'
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init = init[:init.index('    check_templates();')]
    source = OUT/'check.c'
    source.write_text(prefix+checks+init+'checks(); SDL_Quit(); return 0;\n}\n')
    cmake = BUILD/'CMakeFiles/sil-more.dir'
    objects = shlex.split((cmake/'objects1.rsp').read_text())
    response = OUT/'objects.rsp'
    response.write_text('\n'.join('"'+p+'"' for p in objects if not p.endswith('/src/main.c.obj')))
    env = os.environ.copy()
    env['PATH'] = os.pathsep.join([*(str(BUILD/'_deps'/p) for p in ('SDL','SDL_ttf','SDL_image','SDL_mixer')),
        'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    exe = OUT/'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0',
        '@CMakeFiles/sil-more.dir/includes_C.rsp',str(source),str(cleanup_source),'@'+str(response),
        '@CMakeFiles/sil-more.dir/linkLibs.rsp','-Wl,--wrap=handle_stuff,--wrap=sdl_refresh_supporting_panes_layout,--wrap=sdl_suspend_main_view_zoom_for_saved_screen,--wrap=sdl_resume_main_view_zoom_for_saved_screen,--wrap=sdl_push_terminal_menu_scale_for,--wrap=sdl_pop_terminal_menu_scale,--wrap=sdl_music_play_menu_theme,--wrap=sdl_music_stop_main,--wrap=message_flush,--wrap=dismiss_active_narrative_banner,--wrap=sdl_touch_settings_tutorial_requested,--wrap=sdl_mouse_settings_tutorial_requested,--wrap=sdl_character_wheel_settings_tutorial_requested,--wrap=sdl_zones_settings_tutorial_requested,--wrap=sdl_zones_show_requested_tutorial','-o',str(exe)],
        cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-',dir=OUT) as state:
        subprocess.run([str(exe),str(ROOT/'lib/edit'),state],cwd=state,env=env,check=True,timeout=20)


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Run the actual Look loop and preserve the pre-Look view across zoom changes."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build-standard'
OUT = ROOT / 'scripts/output/look-zoom-exit'
CHECKS = r'''
#include "support/screen.h"
#include "sdl-config.h"
extern bool g_unified_look_active;
#undef assert
#define assert(expr) do { if (!(expr)) { fprintf(stderr,"%s: %s line %d\n",scenario,#expr,__LINE__); exit(1); } } while (0)
static const char *scenario="initialization", *inputs;
static int input_count, zoom_scale=1;
void __wrap_tutorial_game_menu(const char *id,const char *text)
{ (void)id; (void)text; }
void __wrap_sdl_refresh_supporting_panes_layout(void) {}
char __wrap_inkey(void)
{
    char key;
    if (!Term_inkey(&key,false,true)) return key;
    assert(inputs && *inputs);
    input_count++;
    assert(input_count<=3);
    return *inputs++;
}
int __wrap_get_sdl_effective_main_view_scale(void) { return zoom_scale; }
int __wrap_get_sdl_min_main_view_zoom_scale(void) { return 1; }
int __wrap_get_sdl_max_main_view_zoom_scale(void) { return 4; }
bool __wrap_set_sdl_main_view_zoom_scale(int scale) { zoom_scale=scale; return true; }
void __wrap_sdl_apply_config_no_redraw(void)
{ assert(!Term_resize(80/zoom_scale,24/zoom_scale)); }
cptr __wrap_get_sdl_config_path(void) { return "fixture-sdl.json"; }
static void check_exit(bool panned,bool change_zoom,bool zoom_out)
{
    scenario=panned?"Panned Look exit":"Visible-player Look exit";
    memset(p_ptr,0,sizeof(*p_ptr)); reset_map(1);
    p_ptr->py=p_ptr->px=44;
    p_ptr->chp=p_ptr->mhp=36; p_ptr->playing=true;
    character_dungeon=character_generated=true; character_icky=0;
    cave_m_idx[44][44]=-1;
    for(int y=0;y<88;y++) for(int x=0;x<88;x++)
        cave_info[y][x]=CAVE_MARK|CAVE_GLOW|CAVE_SEEN|CAVE_VIEW;
    zoom_scale=zoom_out?3:1; (void)Term_resize(80/zoom_scale,24/zoom_scale);
    assert(Term->wid==80/zoom_scale && Term->hgt==24/zoom_scale);
    p_ptr->wy=panned?4:44-SCREEN_HGT/2;
    p_ptr->wx=panned?4:44-SCREEN_WID/2;
    int wy=p_ptr->wy,wx=p_ptr->wx;
    int center_y=wy+SCREEN_HGT/2,center_x=wx+SCREEN_WID/2;
    bool was_visible=panel_contains(44,44);
    bool hide_left=g_hide_left_panel;
    turn=81; playerturn=8;
    inputs=change_zoom?(zoom_out?"--\033":"++\033"):"\033"; input_count=0; Term_flush();
    puts(scenario); do_cmd_unified_look();
    assert(!*inputs && input_count==(change_zoom?3:1));
    assert(character_icky==0 && !screen_saved_fullscreen_active());
    assert(g_hide_left_panel==hide_left && !g_unified_look_active);
    assert(p_ptr->py==44 && p_ptr->px==44 && p_ptr->chp==36);
    assert(turn==81 && playerturn==8);
    if (change_zoom) {
        assert(zoom_scale==(zoom_out?1:3));
        /* Useful off-map panning is intentional; keep its world centre too. */
        assert(p_ptr->wy+SCREEN_HGT/2==center_y);
        assert(p_ptr->wx+SCREEN_WID/2==center_x);
        assert(panel_contains(44,44)==was_visible);
    } else assert(p_ptr->wy==wy && p_ptr->wx==wx);
    printf("%s / zoom %s: original view, player visibility and free exit PASS\n",scenario,change_zoom?"changed":"unchanged");
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]
    prefix += 'static errr terminal_extra(int action,int value) { (void)action; (void)value; return 0; }\n'
    prefix += fixture_function('reset_map') + '\n'
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init = init[:init.index('    check_templates();')]
    source = OUT / 'check.c'
    source.write_text(prefix + CHECKS + init +
        '    check_exit(false,false,false); check_exit(false,true,false); check_exit(true,true,false);\n'
        '    check_exit(false,true,true); check_exit(true,true,true); SDL_Quit(); return 0;\n}\n', encoding='utf-8')
    objects = shlex.split((BUILD / 'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    response = OUT / 'objects.rsp'
    response.write_text('\n'.join('"' + p + '"' for p in objects if not p.endswith('/src/main.c.obj')), encoding='utf-8')
    env = os.environ.copy()
    env['PATH'] = os.pathsep.join([*(str(BUILD / '_deps' / p) for p in ('SDL','SDL_ttf','SDL_image','SDL_mixer')),
        'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    wraps = ('inkey','get_sdl_effective_main_view_scale','get_sdl_min_main_view_zoom_scale',
        'get_sdl_max_main_view_zoom_scale','set_sdl_main_view_zoom_scale','sdl_apply_config_no_redraw','get_sdl_config_path',
        'tutorial_game_menu','sdl_refresh_supporting_panes_layout')
    exe = OUT / 'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0',
        '@CMakeFiles/sil-more.dir/includes_C.rsp',str(source),'@'+str(response),
        '@CMakeFiles/sil-more.dir/linkLibs.rsp',*(f'-Wl,--wrap={name}' for name in wraps),
        '-o',str(exe)],cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-',dir=OUT) as state:
        subprocess.run([str(exe),str(ROOT/'lib/edit'),state],cwd=state,env=env,check=True,timeout=15)


if __name__ == '__main__':
    main()

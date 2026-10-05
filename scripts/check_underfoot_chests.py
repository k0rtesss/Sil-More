#!/usr/bin/env python3
"""Exercise open-command target collection for underfoot chests and doors.

Uses current Windows CMake engine objects and isolated templates/configs.
--baseline restores only the old adjacent-only open picker for verification.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build-standard'
OUT = ROOT / 'scripts/output/underfoot-chests'
CHECKS = r'''
#undef assert
#define assert(expr) do { if (!(expr)) { fprintf(stderr,"Assertion failed: %s line %d\n",#expr,__LINE__); exit(1); } } while (0)
static int calls,chosen_y,chosen_x,choices;
static bool cancel_choice;
bool __wrap_tutorial_game_action_allowed(const char *action,const object_type *item)
{ (void)action; (void)item; return true; }
bool __wrap_do_cmd_open_chest(int y,int x,s16b index)
{
    assert(index==1); calls++; chosen_y=y; chosen_x=x;
    /* Match the production chest menu's free Escape path. */
    p_ptr->energy_use=0; return false;
}
bool __wrap_get_grid_choice_dir(cptr prompt,const int ys[],const int xs[],
    const int dirs[],int count,int *dir)
{
    bool under=false,east=false;
    assert(strstr(prompt,"Open")); assert(count==2); choices++;
    for (int i=0;i<count;i++) {
        if (dirs[i]==5) { under=true; assert(ys[i]==5 && xs[i]==5); }
        if (dirs[i]==6) { east=true; assert(ys[i]==5 && xs[i]==6); }
    }
    assert(under && east);
    if (cancel_choice) return false;
    *dir=5; return true;
}
static void prepare(void)
{
    memset(p_ptr,0,sizeof(*p_ptr));
    reset_map(1); p_ptr->py=p_ptr->px=5;
    p_ptr->cur_map_hgt=p_ptr->cur_map_wid=20;
    p_ptr->chp=p_ptr->mhp=100;
    for (int y=0;y<20;y++) for (int x=0;x<20;x++)
        cave_info[y][x]=CAVE_MARK|CAVE_GLOW|CAVE_SEEN;
    calls=choices=0; cancel_choice=false; repeat_clear();
    chest_trap_minigame=true; lockpick_minigame=false;
}
static void chest_at(int y,int x,int pval)
{
    object_prep(&o_list[1],lookup_kind(TV_CHEST,2));
    o_list[1].number=1; o_list[1].pval=pval;
    o_list[1].iy=y; o_list[1].ix=x; cave_o_idx[y][x]=1; o_max=2;
}
static void checks(void)
{
    prepare(); chest_at(5,5,5); do_cmd_open();
    assert(calls==1 && chosen_y==5 && chosen_x==5 && !p_ptr->energy_use);
    prepare(); chest_at(5,6,5); do_cmd_open();
    assert(calls==1 && chosen_y==5 && chosen_x==6 && !p_ptr->energy_use);
    prepare(); cave_feat[5][6]=FEAT_DOOR_HEAD; do_cmd_open();
    assert(!calls && cave_feat[5][6]==FEAT_OPEN && p_ptr->energy_use==100);
    prepare(); chest_at(5,5,5); cave_feat[5][6]=FEAT_DOOR_HEAD;
    cancel_choice=true; do_cmd_open();
    assert(choices==1 && !calls && !p_ptr->energy_use && cave_feat[5][6]==FEAT_DOOR_HEAD);
    prepare(); chest_at(5,5,5); cave_feat[5][6]=FEAT_DOOR_HEAD; do_cmd_open();
    assert(choices==1 && calls==1 && chosen_y==5 && chosen_x==5);
    prepare(); chest_at(5,5,0); do_cmd_open();
    assert(!calls && !p_ptr->energy_use);
    prepare(); do_cmd_open(); assert(!calls && !p_ptr->energy_use);
    puts("Open targets: underfoot/adjacent chests, adjacent door, mixed selection, free cancellation and empty chest/grid PASS.");
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', action='store_true')
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]
    prefix += fixture_function('terminal_extra') + '\n' + fixture_function('reset_map') + '\n'
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init = init[:init.index('    check_templates();')]
    source = OUT / 'check.c'
    source.write_text(prefix + CHECKS + init + '    checks(); SDL_Quit(); return 0;\n}\n', encoding='utf-8')
    objects = shlex.split((BUILD / 'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    excluded = ['/src/main.c.obj']
    extra = []
    if args.baseline:
        text = (ROOT / 'src/cmd/world/cmd-interact.c').read_text(encoding='utf-8')
        old = 'get_interact_dir("Open what?", grid_is_open_target, true,'
        assert text.count(old) == 1
        baseline = OUT / 'cmd-interact-baseline.c'
        baseline.write_text(text.replace(old, old.replace('true,','false,')), encoding='utf-8')
        extra.append(str(baseline)); excluded.append('/src/cmd/world/cmd-interact.c.obj')
    response = OUT / 'objects.rsp'
    response.write_text('\n'.join('"' + p + '"' for p in objects
                                 if not p.endswith(tuple(excluded))), encoding='utf-8')
    env = os.environ.copy()
    env['PATH'] = os.pathsep.join([
        *(str(BUILD / '_deps' / p) for p in ('SDL', 'SDL_ttf', 'SDL_image', 'SDL_mixer')),
        'C:/msys64/mingw64/bin', 'C:/msys64/usr/bin', env['PATH']])
    exe = OUT / 'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe', '-DUSE_SDL', '-std=c17', '-O0',
                    '@CMakeFiles/sil-more.dir/includes_C.rsp', str(source), *extra, '@' + str(response),
                    '@CMakeFiles/sil-more.dir/linkLibs.rsp',
                    *(f'-Wl,--wrap={name}' for name in ('tutorial_game_action_allowed',
                        'do_cmd_open_chest','get_grid_choice_dir')), '-o', str(exe)],
                   cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-', dir=OUT) as state:
        subprocess.run([str(exe), str(ROOT / 'lib/edit'), state], cwd=state,
                       env=env, check=True, timeout=15)


if __name__ == '__main__':
    main()

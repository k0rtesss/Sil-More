#!/usr/bin/env python3
"""Roundtrip wizard Forget and reject clock resets in temporary saves.

Run build-incremental.ps1 first. The real wizard command and full save/load
functions run against initialized engine data; player files are never opened.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function, TESTS

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build-standard'
OUT = ROOT / 'scripts/output/wizard-forget-save'

CHECKS = r'''
void fixture_forget(void);
static void prepare_world(void)
{
    fresh_map(); turn = 1000;
    p_ptr->playing = p_ptr->wizard = true;
    p_ptr->noscore = 0x000a;
    min_depth_counter = 100;
    SDL_strlcpy(op_ptr->full_name, "ForgetFixture", sizeof(op_ptr->full_name));
    process_player_name(true);
    cave_set_feat(10, 10, FEAT_WATER);
    cave_environment_seed();
    cave_event_emit(CAVE_EVENT_BUILD, 8, 8, 10);
    character_dungeon = character_generated = true;
}

static void check_forget_roundtrip(void)
{
    prepare_world();
    environment_state before = cave_environment_get_state();
    environment_source source = *cave_environment_source_at(0);
    cave_world_event events[CAVE_EVENTS_MAX];
    memcpy(events, cave_events_state(), sizeof(events));
    fixture_forget();
    assert(turn == 1000 && playerturn == 1 && min_depth_counter == 0);
    assert(p_ptr->noscore == 0 && p_ptr->wizard);
    assert(save_player());
    assert(load_player() && character_loaded && !p_ptr->is_dead);
    environment_state after = cave_environment_get_state();
    assert(after.last_turn == before.last_turn && after.random == before.random);
    assert(!memcmp(cave_environment_source_at(0), &source, sizeof(source)));
    assert(!memcmp(cave_events_state(), events, sizeof(events)));
    puts("Wizard Forget: world clock, source deadlines, events and full save/load PASS.");
}

static void check_clock_reset_rejected(void)
{
    prepare_world();
    turn = playerturn = 1;
    p_ptr->noscore = 0; min_depth_counter = 0;
    assert(save_player());
    assert(!load_player());
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]
    prefix += '#include "cave/cave-environment.h"\n#include "cave/cave-events.h"\n'
    prefix += '#include "cave/cave-fixtures.h"\n#include "cave/cave-flood.h"\n'
    prefix += '#include "cave/cave-water-flow.h"\n'
    prefix += 'cptr __wrap_get_sdl_config_path(void) { return "fixture-sdl.json"; }\n'
    prefix += fixture_function('terminal_extra') + '\n' + fixture_function('reset_map') + '\n'
    start = TESTS.index('static void fresh_map(void)')
    prefix += TESTS[start:TESTS.index('\n}', start) + 2] + '\n' + CHECKS
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init = init[:init.index('    check_templates();')]
    init = init.replace('    assert(init_z_info()==0);', '    assert(init_z_info()==0); assert(init_rt_info()==0);')
    init = init.replace('assert(init_style_info()==0); assert(init_other()==0);',
        'assert(init_h_info()==0); assert(init_st_info()==0); assert(init_partition_info()==0); '
        'assert(init_quest_info()==0); assert(init_effect_info()==0); assert(init_flavor_info()==0); '
        'assert(init_skeleton_note_info()==0); assert(init_n_info()==0); '
        'assert(init_style_info()==0); assert(init_other()==0);')
    init += '    ANGBAND_DIR_SAVE = ANGBAND_DIR_APEX = argv[2];\n'
    init += '    player_wipe(); build_randart_tables(); flavor_init();\n'
    init += '    check_forget_roundtrip();\n'
    init += '    check_clock_reset_rejected();\n'
    init += '    puts("Invalid clock reset rejection PASS.");\n'
    init += '    SDL_Quit(); return 0;\n}\n'
    source = OUT / 'check.c'
    source.write_text(prefix + '\n' + init, encoding='utf-8')
    wizard = OUT / 'wizard.c'
    wizard.write_text('#include "wizard2.c"\nvoid fixture_forget(void) { do_cmd_wiz_forget(); }\n')
    objects = shlex.split((BUILD / 'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    excluded = ('/src/main.c.obj', '/src/wizard2.c.obj')
    response = OUT / 'objects.rsp'
    response.write_text('\n'.join('"' + p + '"' for p in objects if not p.endswith(excluded)))
    env = os.environ.copy()
    env['PATH'] = os.pathsep.join([
        *(str(BUILD / '_deps' / p) for p in ('SDL', 'SDL_ttf', 'SDL_image', 'SDL_mixer')),
        'C:/msys64/mingw64/bin', 'C:/msys64/usr/bin', env['PATH']])
    exe = OUT / 'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe', '-DUSE_SDL', '-std=c17', '-O0',
        '@CMakeFiles/sil-more.dir/includes_C.rsp', str(source), str(wizard), '@' + str(response),
        '@CMakeFiles/sil-more.dir/linkLibs.rsp', '-Wl,--wrap=get_sdl_config_path',
        '-o', str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-', dir=OUT) as state:
        subprocess.run([str(exe), str(ROOT / 'lib/edit'), state], cwd=state,
            env=env, check=True, timeout=60)


if __name__ == '__main__':
    main()

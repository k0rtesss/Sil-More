#!/usr/bin/env python3
"""Check paid level exits and fatal end-of-turn damage in real player commands.

Uses current CMake objects and isolated engine state. Input/confirmation and
free non-stair exits are simulated at the command boundary.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build-standard'
OUT = ROOT / 'scripts/output/level-transition-turns'

CHECKS = r'''
#undef assert
#define assert(expr) do { if (!(expr)) { fprintf(stderr, "%s: assertion failed: %s (%d)\n", scenario, #expr, __LINE__); exit(1); } } while (0)
static const char *scenario = "initialization";
static int requests, commands, action;
static int oath_curses;
static bool allow_confirmation, accept_oath;
extern void process_player(void);
bool __wrap_varda_quest_confirm_leave_bastion(void) { return allow_confirmation; }
bool __wrap_get_check_oath_multiline(cptr prompt) { (void)prompt; return accept_oath; }
void __wrap_apply_oath_breaking_curse(int oath) { assert(oath == OATH_IRON); oath_curses++; }
void __wrap_request_command(void)
{
    requests++;
    /* A cancelled/free command returns to the next input prompt. End the
     * fixture there through the ordinary zero-energy quit boundary. */
    if (requests > 1) { p_ptr->playing = false; p_ptr->leaving = true; }
}
void __wrap_process_command(void)
{
    commands++;
    assert(commands == 1);
    switch (action) {
    case 1: do_cmd_go_up(); break;
    case 2: do_cmd_go_down(); break;
    case 3: p_ptr->playing = false; p_ptr->leaving = true; break;
    case 4: /* Save is free and remains at the command prompt. */ break;
    case 5: /* Wizard jump changes the level without taking energy. */
        p_ptr->depth = 12; p_ptr->leaving = true; break;
    case 6: /* Other paid escapes/deaths use the same exit boundary. */
        p_ptr->energy_use = 100; p_ptr->leaving = true; break;
    case 7: do_cmd_hold(); break;
    default: assert(false);
    }
}
cptr __wrap_get_sdl_config_path(void) { return "fixture-sdl.json"; }
static void prepare(const char *name, int feature, int chosen_action)
{
    scenario = name; puts(name);
    memset(p_ptr, 0, sizeof(*p_ptr));
    reset_map(10);
    p_ptr->py = p_ptr->px = 5;
    p_ptr->cur_map_hgt = 20; p_ptr->cur_map_wid = 24;
    p_ptr->chp = p_ptr->mhp = 100;
    p_ptr->food = PY_FOOD_FULL;
    p_ptr->depth = p_ptr->max_depth = 10;
    p_ptr->energy = 100;
    p_ptr->playing = true;
    cave_m_idx[5][5] = -1;
    for (int y = 0; y < 20; y++) for (int x = 0; x < 24; x++) {
        bool border = !y || !x || y == 19 || x == 23;
        cave_feat[y][x] = border ? FEAT_WALL_PERM : FEAT_FLOOR;
        cave_info[y][x] = border ? CAVE_WALL : CAVE_MARK | CAVE_GLOW | CAVE_SEEN;
    }
    cave_feat[5][5] = feature;
    playerturn = 50; min_depth_counter = 0;
    character_dungeon = character_generated = true;
    requests = commands = oath_curses = 0; action = chosen_action;
    allow_confirmation = accept_oath = true;
}
static void run(bool paid, int expected_depth)
{
    process_player();
    assert(commands == 1 && p_ptr->leaving);
    assert(p_ptr->depth == expected_depth);
    assert(p_ptr->energy_use == (paid ? 100 : 0));
    assert(p_ptr->energy == (paid ? 0 : 100));
    assert(playerturn == (paid ? 51 : 50));
    assert(requests == (paid || action == 3 || action == 5 ? 1 : 2));
}
static void check_upkeep_death(cptr cause, int poison, int cut, int food)
{
    prepare(cause, FEAT_FLOOR, 7);
    p_ptr->song1 = p_ptr->song2 = SNG_NOTHING;
    p_ptr->chp = 1;
    p_ptr->poisoned = poison;
    p_ptr->cut = cut;
    p_ptr->food = food;
    p_ptr->diseased = 2;
    p_ptr->slow = 4;
    process_player();
    assert(p_ptr->is_dead && p_ptr->leaving && streq(p_ptr->died_from, cause));
    assert(commands == 1 && playerturn == 51 && p_ptr->energy == 0);
    assert(p_ptr->diseased == 2 && p_ptr->slow == 4);
    assert(p_ptr->poisoned == poison && p_ptr->cut == cut);
    assert(p_ptr->food == (poison || cut ? food : food - 1));
}

static void check_surviving_upkeep(void)
{
    prepare("Surviving poison still advances ailment timers", FEAT_FLOOR, 7);
    p_ptr->song1 = p_ptr->song2 = SNG_NOTHING;
    p_ptr->chp = 20;
    p_ptr->poisoned = p_ptr->cut = 1;
    p_ptr->slow = 4;
    p_ptr->diseased = 10;
    process_player();
    assert(!p_ptr->is_dead && p_ptr->chp == 18 && playerturn == 51);
    assert(!p_ptr->poisoned && !p_ptr->cut && p_ptr->slow == 3 && p_ptr->diseased == 8);

    prepare("Last Chances rescues the player and the turn completes", FEAT_FLOOR, 7);
    p_ptr->song1 = p_ptr->song2 = SNG_NOTHING;
    p_ptr->chp = p_ptr->poisoned = 1;
    p_ptr->diseased = 10;
    object_prep(&inventory[INVEN_NECK], lookup_kind(TV_AMULET, SV_AMULET_LAST_CHANCES));
    p_ptr->equip_cnt = 1;
    process_player();
    assert(!p_ptr->is_dead && !p_ptr->leaving && p_ptr->chp > 0);
    assert(!inventory[INVEN_NECK].k_idx && !p_ptr->poisoned);
    assert(p_ptr->diseased == 8 && playerturn == 51);
}

static void checks(void)
{
    prepare("Upstairs counts one paid action", FEAT_LESS, 1); run(true, 9);
    prepare("Downstairs counts one paid action", FEAT_MORE, 2); run(true, 11);
    prepare("Up shaft counts one action, not two depths", FEAT_LESS_SHAFT, 1); run(true, 8);
    prepare("Down shaft counts one action, not two depths", FEAT_MORE_SHAFT, 2); run(true, 12);
    prepare("Refused ascent without stairs is free", FEAT_FLOOR, 1); run(false, 10);
    prepare("Refused descent without stairs is free", FEAT_FLOOR, 2); run(false, 10);
    prepare("Cancelled stair confirmation is free", FEAT_MORE, 2); allow_confirmation = false; run(false, 10);
    prepare("Cancelling the final ascent prompt preserves the Iron oath", FEAT_LESS, 1);
    p_ptr->oath_type = OATH_IRON; allow_confirmation = false; run(false, 10);
    assert(!oath_invalid(OATH_IRON) && !oath_curses);
    prepare("Declining to break the Iron oath is free", FEAT_LESS, 1);
    p_ptr->oath_type = OATH_IRON; accept_oath = false; run(false, 10);
    assert(!oath_invalid(OATH_IRON) && !oath_curses);
    prepare("Confirmed ascent breaks the Iron oath once", FEAT_LESS, 1);
    p_ptr->oath_type = OATH_IRON; run(true, 9);
    assert(oath_invalid(OATH_IRON) && oath_curses == 1);
    prepare("A blocked throne-room exit preserves the Iron oath", FEAT_LESS, 1);
    p_ptr->depth = MORGOTH_DEPTH; p_ptr->morgoth_hall_entered = true;
    p_ptr->oath_type = OATH_IRON; do_cmd_go_up();
    assert(p_ptr->depth == MORGOTH_DEPTH && !p_ptr->leaving && p_ptr->energy_use == 100);
    assert(!oath_invalid(OATH_IRON) && !oath_curses);
    prepare("Quit exit is free", FEAT_FLOOR, 3); run(false, 10);
    prepare("Save then quit remains free", FEAT_FLOOR, 4); run(false, 10);
    prepare("Wizard jump exit is free", FEAT_FLOOR, 5); run(false, 12);
    prepare("Other paid exit counts once", FEAT_FLOOR, 6); run(true, 10);
    check_upkeep_death("poison", 1, 1, PY_FOOD_FULL - 100);
    check_upkeep_death("a fatal wound", 0, 1, PY_FOOD_FULL - 100);
    check_upkeep_death("starvation", 0, 0, PY_FOOD_STARVE);
    check_surviving_upkeep();
    puts("Level transition player-turn accounting: PASS.");
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]
    prefix += fixture_function('terminal_extra') + '\n' + fixture_function('reset_map') + '\n'
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init = init[:init.index('    check_templates();')]
    source = OUT / 'check.c'
    source.write_text(prefix + CHECKS + init + '    assert(init_flavor_info() == 0); flavor_init();\n'
        '    checks(); SDL_Quit(); return 0;\n}\n', encoding='utf-8')
    objects = shlex.split((BUILD / 'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    response = OUT / 'objects.rsp'
    response.write_text('\n'.join('"' + p + '"' for p in objects if not p.endswith('/src/main.c.obj')), encoding='utf-8')
    env = os.environ.copy()
    env['PATH'] = os.pathsep.join([*(str(BUILD / '_deps' / p) for p in ('SDL', 'SDL_ttf', 'SDL_image', 'SDL_mixer')),
        'C:/msys64/mingw64/bin', 'C:/msys64/usr/bin', env['PATH']])
    exe = OUT / 'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe', '-DUSE_SDL', '-std=c17', '-O0',
        '@CMakeFiles/sil-more.dir/includes_C.rsp', str(source), '@' + str(response),
        '@CMakeFiles/sil-more.dir/linkLibs.rsp',
        *(f'-Wl,--wrap={name}' for name in ('request_command', 'process_command',
            'varda_quest_confirm_leave_bastion', 'get_sdl_config_path',
            'get_check_oath_multiline', 'apply_oath_breaking_curse')),
        '-o', str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-', dir=OUT) as state:
        subprocess.run([str(exe), str(ROOT / 'lib/edit'), state], cwd=state, env=env, check=True, timeout=15)


if __name__ == '__main__':
    main()

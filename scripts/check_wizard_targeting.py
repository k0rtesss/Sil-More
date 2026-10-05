#!/usr/bin/env python3
"""Exercise the real wizard dungeon editor against an isolated engine fixture.

Run build-incremental.ps1 first. Links current CMake engine objects and initializes
shipped templates in a temporary directory; no normal save/config is opened.
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
OUT = ROOT / 'scripts/output/wizard-targeting'

CHECKS = r'''
#undef assert
#define assert(expr) do { if (!(expr)) { fprintf(stderr, "%s: assertion failed: %s (line %d)\n", scenario ? scenario : "initialization", #expr, __LINE__); exit(1); } } while (0)
#include "support/input.h"
#include "support/movement-input.h"
#include "ui/menu-click.h"

static const char *scenario;
static const char *keys;
static unsigned key_index, key_count;
static int bells, deleted, placed;
static bool typed_west;
static int placed_y, placed_x;
char __real_inkey_movement_context(u16b context);

char __wrap_inkey_movement_context(u16b context)
{
    assert(context == MOVEMENT_INPUT_CONTEXT_TARGETING);
    if (key_index >= key_count) {
        fprintf(stderr, "%s: unexpected extra input prompt\n", scenario);
        exit(1);
    }
    char key = keys[key_index++];
    if (key == '`') {
        /* Exercise the real reader: its normal path maps backtick to Escape.
         * The wizard-only raw scope must preserve the actual queued byte. */
        assert(inkey_base);
        Term_flush();
        inkey_xtra = false;
        inkey_next_set(NULL);
        assert(Term_keypress('`') == 0);
        key = __real_inkey_movement_context(context);
        assert(key == '`');
    }
    if (typed_west && key == UI_MENU_CLICK_WAKE_KEY) {
        movement_input_command command = {0};
        command.context = MOVEMENT_INPUT_CONTEXT_TARGETING;
        command.action = MOVEMENT_INPUT_ACTION_MOVE_DIR;
        command.direction = MOVEMENT_INPUT_DIRECTION_WEST;
        assert(movement_input_submit_command(&command));
    }
    return key;
}
void __wrap_bell(cptr reason)
{
    (void)reason;
    bells++;
}
void __real_delete_monster_idx(int index);
void __wrap_delete_monster_idx(int index)
{
    /* Fail before the historical negative-index memory access/hang. */
    assert(index > 0);
    deleted++;
    __real_delete_monster_idx(index);
}
bool __real_place_monster_one(int y, int x, int race, bool sleep,
    bool ignore_depth, monster_type *monster);
bool __wrap_place_monster_one(int y, int x, int race, bool sleep,
    bool ignore_depth, monster_type *monster)
{
    assert(cave_m_idx[y][x] != -1);
    assert(y != p_ptr->py || x != p_ptr->px);
    placed++; placed_y = y; placed_x = x;
    return __real_place_monster_one(y, x, race, sleep, ignore_depth, monster);
}
static void prepare_editor(const char *name, const char *sequence, unsigned count)
{
    scenario = name;
    puts(name);
    memset(p_ptr, 0, sizeof(*p_ptr));
    reset_map(10);
    p_ptr->py = p_ptr->px = 5;
    p_ptr->cur_map_hgt = 20; p_ptr->cur_map_wid = 24;
    p_ptr->chp = p_ptr->mhp = 100;
    p_ptr->wizard = p_ptr->playing = true;
    p_ptr->noscore = 0x000a;

    cave_m_idx[5][5] = -1;
    for (int y = 0; y < 20; y++) for (int x = 0; x < 24; x++) {
        bool border = !y || !x || y == 19 || x == 23;
        cave_feat[y][x] = border ? FEAT_WALL_PERM : FEAT_FLOOR;
        cave_info[y][x] = border ? CAVE_WALL : CAVE_MARK | CAVE_GLOW | CAVE_SEEN;
    }
    character_dungeon = character_generated = true;
    keys = sequence; key_count = count; key_index = 0;
    bells = deleted = placed = 0; typed_west = false;
    movement_input_clear_commands();
}
static void run_editor(void)
{
    (void)target_set_interactive(TARGET_WIZ, 0);
    assert(key_index == key_count);
    assert(p_ptr->py == 5 && p_ptr->px == 5);
    assert(cave_m_idx[5][5] == -1);
    assert(mon_cnt >= 0);
    assert(!inkey_base);
}
static void check_numeric_movement(void)
{
    const char input[] = {'4', '<', ESCAPE};
    prepare_editor("Numeric movement and ordinary-cell terrain edit", input, sizeof(input));
    run_editor();
    assert(cave_feat[5][4] == FEAT_LESS);
    assert(cave_feat[5][5] == FEAT_FLOOR);
    assert(!bells && !deleted && !placed && mon_cnt == 0);
}
static void check_typed_movement(void)
{
    /* The SDL keypad/arrow route submits a typed direction and wakes input with
     * this byte. This is the physical KP4 representation observed in gameplay. */
    const char input[] = {UI_MENU_CLICK_WAKE_KEY, '<', ESCAPE};
    prepare_editor("Physical keypad/arrow movement keeps edits off player", input, sizeof(input));
    typed_west = true;
    run_editor();
    assert(cave_feat[5][4] == FEAT_LESS);
    assert(cave_feat[5][5] == FEAT_FLOOR);
    assert(!bells && !deleted && !placed && mon_cnt == 0);
}
static void check_player_edit(void)
{
    const char terrain[] = {'<', ESCAPE};
    prepare_editor("Terrain edit under player preserves occupancy and monster count", terrain, sizeof(terrain));
    run_editor();
    assert(cave_feat[5][5] == FEAT_LESS);
    assert(!bells && !deleted && !placed && mon_cnt == 0 && mon_max == 1);

    const char space[] = {' ', ESCAPE};
    prepare_editor("Space cycles player terrain without reading a monster", space, sizeof(space));
    run_editor();
    assert(!deleted && !placed && mon_cnt == 0 && mon_max == 1);

    const char tab[] = {'\t', ESCAPE};
    prepare_editor("Tab on player does not reroll a monster", tab, sizeof(tab));
    run_editor();
    assert(cave_feat[5][5] == FEAT_FLOOR);
    assert(!bells && !deleted && !placed && mon_cnt == 0 && mon_max == 1);

    const char glyph[] = {'h', ESCAPE};
    prepare_editor("Monster glyph on player is rejected safely", glyph, sizeof(glyph));
    run_editor();
    assert(!deleted && !placed && mon_cnt == 0 && mon_max == 1);
    assert(cave_feat[5][5] == FEAT_FLOOR);
}
static void check_normal_monster_edit(void)
{
    /* In vi key mode h must remain a monster glyph, not a movement binding. */
    hjkl_movement = true;
    r_info[41].d_char = 'h';
    const char input[] = {'4', 'h', ESCAPE};
    prepare_editor("Monster glyph on ordinary cell remains available in vi mode", input, sizeof(input));
    run_editor();
    assert(placed == 1 && placed_y == 5 && placed_x == 4);
    assert(cave_m_idx[5][4] > 0 && mon_cnt == 1);
    assert(!bells && !deleted);
    const char replace[] = {'4', 'h', '<', ESCAPE};
    prepare_editor("Terrain replacement deletes an ordinary monster safely", replace, sizeof(replace));
    run_editor();
    assert(placed == 1 && deleted == 1 && mon_cnt == 0);
    assert(cave_m_idx[5][4] == 0 && cave_feat[5][4] == FEAT_LESS);
    assert(!bells);
    hjkl_movement = false;
}
static void check_escape(void)
{
    const char input[] = {ESCAPE};
    prepare_editor("Escape exits without an illegal-command bell", input, sizeof(input));
    run_editor();
    assert(!bells && !deleted && !placed && mon_cnt == 0);
}
static void check_lava_shortcut(void)
{
    const char input[] = {'4', '`', ESCAPE};
    prepare_editor("Backtick creates lava on an ordinary cell", input, sizeof(input));
    run_editor();
    assert(cave_feat[5][4] == FEAT_LAVA);
    assert(cave_feat[5][5] == FEAT_FLOOR);
    assert(!bells && !deleted && !placed && mon_cnt == 0);

    const char player[] = {'`', ESCAPE};
    prepare_editor("Lava edit under player preserves negative occupancy", player, sizeof(player));
    run_editor();
    assert(cave_feat[5][5] == FEAT_LAVA && cave_m_idx[5][5] == -1);
    assert(!bells && !deleted && !placed && mon_cnt == 0);

    r_info[41].d_char = 'h';
    const char replace[] = {'4', 'h', '`', ESCAPE};
    prepare_editor("Lava edit replaces an ordinary monster safely", replace, sizeof(replace));
    run_editor();
    assert(cave_feat[5][4] == FEAT_LAVA && cave_m_idx[5][4] == 0);
    assert(placed == 1 && deleted == 1 && mon_cnt == 0 && !bells);

    /* Leaving the editor must restore ordinary Escape aliases. */
    assert(!inkey_base);
    Term_flush();
    inkey_xtra = false;
    inkey_next_set(NULL);
    assert(Term_keypress('`') == 0);
    assert(__real_inkey_movement_context(MOVEMENT_INPUT_CONTEXT_TARGETING) == ESCAPE);
    assert(!inkey_base);
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", action="store_true", help="Compile committed targeting source to verify the regression fails safely")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]
    prefix += fixture_function('terminal_extra') + '\n' + fixture_function('reset_map') + '\n'
    prefix += 'cptr __wrap_get_sdl_config_path(void) { return "fixture-sdl.json"; }\n'
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init = init[:init.index('    check_templates();')]
    if args.baseline:
        init += '    check_player_edit();\n'
    else:
        init += '    check_numeric_movement(); check_typed_movement(); check_player_edit();\n'
    init += '    check_normal_monster_edit(); check_escape(); check_lava_shortcut();\n'
    init += '    puts("Wizard targeting integration: PASS."); SDL_Quit(); return 0;\n}\n'
    source = OUT / 'check.c'
    source.write_text(prefix + CHECKS + init, encoding='utf-8')
    objects = shlex.split((BUILD / 'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    extra_sources = []
    excluded = ['/src/main.c.obj']
    if args.baseline:
        baseline = OUT / 'targeting-baseline.c'
        baseline.write_text(subprocess.check_output(['git', '-c', 'safe.directory=' + ROOT.as_posix(), 'show', 'HEAD:src/ui/targeting/targeting.c'], cwd=ROOT, text=True), encoding='utf-8')
        extra_sources.append(str(baseline))
        excluded.append('/src/ui/targeting/targeting.c.obj')
    response = OUT / 'objects.rsp'
    response.write_text('\n'.join('"' + p + '"' for p in objects
        if not p.endswith(tuple(excluded))), encoding='utf-8')
    env = os.environ.copy()
    env['PATH'] = os.pathsep.join([
        *(str(BUILD / '_deps' / p) for p in ('SDL', 'SDL_ttf', 'SDL_image', 'SDL_mixer')),
        'C:/msys64/mingw64/bin', 'C:/msys64/usr/bin', env['PATH']])
    exe = OUT / 'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe', '-DUSE_SDL', '-std=c17', '-O0',
        '@CMakeFiles/sil-more.dir/includes_C.rsp', str(source), *extra_sources, '@' + str(response),
        '@CMakeFiles/sil-more.dir/linkLibs.rsp',
        *(f'-Wl,--wrap={name}' for name in ('inkey_movement_context', 'bell',
            'delete_monster_idx', 'place_monster_one', 'get_sdl_config_path')),
        '-o', str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-', dir=OUT) as state:
        subprocess.run([str(exe), str(ROOT / 'lib/edit'), state], cwd=state,
            env=env, check=True, timeout=15)


if __name__ == '__main__':
    main()



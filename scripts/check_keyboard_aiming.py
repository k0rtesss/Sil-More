#!/usr/bin/env python3
"""Check physical keyboard preset routing through the actual aim selector.

Build first. Uses isolated templates and current CMake objects; no real saves.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build-standard'
OUT = ROOT / 'scripts/output/keyboard-aiming'

CHECKS = r'''
#include "sdl-config.h"
#include "support/movement-input.h"
#include "ui/menu-click.h"
#undef assert
#define assert(expr) do { if (!(expr)) { fprintf(stderr, "assertion failed: %s (%d)\n", #expr, __LINE__); exit(1); } } while (0)
extern bool target_select_aim(int range, bool vertical, int *dir);
extern bool target_select_location(cptr action, int *y, int *x);
extern bool sdl_try_send_movement_event(const SDL_KeyboardEvent *event);
static int step, captured_key, bells;
static bool cancel_selection, controller_direction;
static SDL_Scancode direction_key;

errr __wrap_Term_keypress(int key) { captured_key = key; return 0; }
void __wrap_bell(cptr reason) { (void)reason; bells++; }
cptr __wrap_get_sdl_config_path(void) { return "fixture-sdl.json"; }
char __wrap_inkey_movement_context(u16b context)
{
    assert(context == MOVEMENT_INPUT_CONTEXT_TARGETING);
    assert(step < 2);
    if (step++ == 0) {
        if (controller_direction) {
            movement_input_command cmd = {context, MOVEMENT_INPUT_ACTION_MOVE_DIR,
                MOVEMENT_INPUT_DIRECTION_EAST, 0};
            assert(movement_input_submit_command(&cmd));
            return UI_MENU_CLICK_WAKE_KEY;
        }
        SDL_KeyboardEvent event = {0};
        event.scancode = direction_key;
        captured_key = 0;
        assert(sdl_try_send_movement_event(&event));
        /* Physical keyboard direction must be a cursor key, not the semantic
         * controller wake byte which would immediately consume a shot. */
        assert(captured_key == '6');
        return (char)captured_key;
    }
    assert(!controller_direction);
    return cancel_selection ? ESCAPE : '\r';
}

static void prepare(int preset, SDL_Scancode key)
{
    memset(p_ptr, 0, sizeof(*p_ptr));
    reset_map(10);
    p_ptr->py = p_ptr->px = 5;
    p_ptr->cur_map_hgt = 20; p_ptr->cur_map_wid = 24;
    p_ptr->wizard = p_ptr->playing = true;
    p_ptr->chp = p_ptr->mhp = 100;
    cave_m_idx[5][5] = -1;
    for (int y = 0; y < 20; y++) for (int x = 0; x < 24; x++) {
        bool border = !y || !x || y == 19 || x == 23;
        cave_feat[y][x] = border ? FEAT_WALL_PERM : FEAT_FLOOR;
        cave_info[y][x] = border ? CAVE_WALL : CAVE_MARK | CAVE_GLOW | CAVE_SEEN | CAVE_FIRE;
    }
    character_dungeon = character_generated = true;
    character_icky = 0;
    step = bells = 0; direction_key = key;
    movement_input_clear_commands();
    movement_input_set_active_context(MOVEMENT_INPUT_CONTEXT_TARGETING);
    sdl_config_set_default_movement_bindings(&config, preset);
}
static void check_aiming(void)
{
    const int presets[] = {SDL_MOVEMENT_PRESET_CLASSIC_SIL,
        SDL_MOVEMENT_PRESET_MODERN_ARROWS, SDL_MOVEMENT_PRESET_MODERN_WASD_QEZC,
        SDL_MOVEMENT_PRESET_VI_KEYS};
    const SDL_Scancode keys[] = {SDL_SCANCODE_KP_6, SDL_SCANCODE_RIGHT,
        SDL_SCANCODE_D, SDL_SCANCODE_L};
    for (unsigned i = 0; i < N_ELEMENTS(presets); i++) {
        for (int cancel = 0; cancel <= 1; cancel++) {
            prepare(presets[i], keys[i]);
            cancel_selection = cancel; controller_direction = false;
            int dir = 0;
            assert(target_select_aim(10, false, &dir) == !cancel);
            assert(step == 2 && !bells);
            assert(p_ptr->energy_use == 0 && p_ptr->py == 5 && p_ptr->px == 5);
            if (!cancel) {
                assert(dir == 5 && p_ptr->target_set);
                assert(p_ptr->target_row == 5 && p_ptr->target_col == 6);
            }
        }
        /* Map/location prompts use the same targeting context and must still
         * move the cursor rather than commit on a physical direction key. */
        prepare(presets[i], keys[i]);
        cancel_selection = false; controller_direction = false;
        int y = 0, x = 0;
        assert(target_select_location("Choose", &y, &x));
        assert(step == 2 && y == 5 && x == 6 && !bells);

        /* Outside targeting the existing semantic route remains intact. */
        const u16b contexts[] = {MOVEMENT_INPUT_CONTEXT_DUNGEON,
            MOVEMENT_INPUT_CONTEXT_DIRECTION_PROMPT};
        for (unsigned j = 0; j < N_ELEMENTS(contexts); j++) {
            prepare(presets[i], keys[i]);
            movement_input_set_active_context(contexts[j]);
            SDL_KeyboardEvent event = {0}; event.scancode = keys[i];
            captured_key = 0;
            assert(sdl_try_send_movement_event(&event));
            assert(captured_key == UI_MENU_CLICK_WAKE_KEY);
            int direction = 0;
            assert(movement_input_take_legacy_direction(contexts[j], &direction));
            assert(direction == 6);
        }
    }
    /* Controller/touch-style semantic directions retain immediate shots. */
    prepare(SDL_MOVEMENT_PRESET_CLASSIC_SIL, SDL_SCANCODE_KP_6);
    controller_direction = true;
    int dir = 0;
    assert(target_select_aim(10, false, &dir));
    assert(step == 1 && dir == 6 && !bells && !p_ptr->target_set);
    puts("Keyboard aiming: all four presets confirm/cancel and location prompts; gameplay/direction/controller routing preserved. PASS.");
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]
    prefix += fixture_function('terminal_extra') + '\n' + fixture_function('reset_map') + '\n'
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init = init[:init.index('    check_templates();')]
    source = OUT / 'check.c'
    source.write_text(prefix + CHECKS + init + '    check_aiming(); SDL_Quit(); return 0;\n}\n', encoding='utf-8')
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
        *(f'-Wl,--wrap={name}' for name in ('inkey_movement_context', 'Term_keypress', 'bell', 'get_sdl_config_path')),
        '-o', str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-', dir=OUT) as state:
        subprocess.run([str(exe), str(ROOT / 'lib/edit'), state], cwd=state, env=env, check=True, timeout=15)


if __name__ == '__main__':
    main()

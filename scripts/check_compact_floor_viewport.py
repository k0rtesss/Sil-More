#!/usr/bin/env python3
"""Exercise full floor stacks in compact and wide real item selectors."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build-standard'
OUT = ROOT / 'scripts/output/compact-floor-viewport'

CHECKS = r'''
#include "ui/menu-click.h"
#undef assert
#define assert(expr) do { if (!(expr)) { fprintf(stderr, "Failed %s line %d\n", #expr, __LINE__); exit(1); } } while (0)
static bool touch, filtered;
static int scenario, input_count;
void __wrap_message_flush(void) {}
void __wrap_handle_stuff(void) {}
bool __wrap_sdl_touch_only_device_active(void) { return touch; }
static bool accept_floor(const object_type* object)
{
    return !filtered || (object != &o_list[1] && object != &o_list[2]
        && object != &o_list[3]);
}
static void check_last_row(void)
{
    char row[256];
    int y = MIN(filtered ? MAX_FLOOR_STACK - 3 : MAX_FLOOR_STACK,
        Term->hgt - 1);
    assert(Term->wid < (int)sizeof(row));
    memcpy(row, Term->scr->c[y], Term->wid);
    for (int i = 0; i < Term->wid; i++)
        if (!row[i]) row[i] = ' ';
    row[Term->wid] = '\0';
    if (!strstr(row, "(w)"))
        fprintf(stderr, "scenario %d, row %d: '%s'\n", scenario, y, row);
    assert(strstr(row, "(w)")); /* The 23rd original floor index. */
}
char __wrap_inkey(void)
{
    if (scenario == 0 || scenario == 1 || scenario == 4) {
        int moves = filtered ? 19 : 22;
        if (input_count++ < moves) return '2';
        check_last_row();
        return scenario == 4 ? '6' : '\r';
    }
    if (scenario == 5) {
        if (input_count++ == 0) return '8';
        check_last_row();
        return '\r';
    }
    if (scenario == 2 || scenario == 6) {
        if (input_count++ == 0) {
            assert(ui_scroll_area_offset_scroll(100));
            return UI_MENU_CLICK_WAKE_KEY;
        }
        check_last_row();
        assert(ui_menu_click_handle_cell_action(0, scenario == 6 ? 1 : 15,
            UI_MENU_CLICK_PRIMARY));
        return UI_MENU_CLICK_WAKE_KEY;
    }
    input_count++;
    return scenario == 7 ? 'w' : '-';
}
static void check(void)
{
    assert(!Term_resize(50, 16));
    memset(p_ptr, 0, sizeof(*p_ptr));
    p_ptr->py = p_ptr->px = 2;
    p_ptr->cur_map_hgt = p_ptr->cur_map_wid = 20;
    p_ptr->playing = true;
    rp_ptr = &p_info[0];
    current_character_profile = &c_info[0];
    int kind = lookup_kind(TV_ARROW, 1);
    assert(kind);
    for (int i = 1; i <= MAX_FLOOR_STACK; i++) {
        object_prep(&o_list[i], kind);
        o_list[i].number = 1;
        o_list[i].iy = o_list[i].ix = 2;
        o_list[i].marked = true;
        o_list[i].next_o_idx = i < MAX_FLOOR_STACK ? i + 1 : 0;
    }
    o_max = MAX_FLOOR_STACK + 1;
    cave_o_idx[2][2] = 1;
    for (scenario = 0; scenario < 8; scenario++) {
        int chosen = 0;
        touch = scenario == 2 || scenario == 6;
        filtered = scenario == 1;
        item_tester_hook = accept_floor;
        input_count = 0;
        p_ptr->command_see = false;
        assert(get_item(&chosen, "Choose: ", "No items", USE_FLOOR));
        assert(chosen == (scenario == 3 ? -1
            : scenario == 6 ? -9 : -MAX_FLOOR_STACK));
    }
    puts("50x16 floor selector: keyboard/confirm/wrap, filtered items, touch scroll/tap, letters and '-' shortcut PASS.");
    const int wide_columns[] = {92, 93, 120, 180};
    for (int i = 0; i < (int)N_ELEMENTS(wide_columns); i++) {
        int chosen = 0;
        assert(!Term_resize(wide_columns[i], 32));
        scenario = 0;
        touch = filtered = false;
        item_tester_hook = accept_floor;
        input_count = 0;
        p_ptr->command_see = false;
        assert(get_item(&chosen, "Choose: ", "No items", USE_FLOOR));
        assert(chosen == -MAX_FLOOR_STACK);
        printf("%dx32 floor selector: full list and bounded descriptions PASS.\n",
            wide_columns[i]);
    }
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]
    prefix += fixture_function('terminal_extra') + '\n'
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init = init[:init.index('    check_templates();')]
    source = OUT / 'check.c'
    source.write_text(prefix + CHECKS + init +
                      '    check(); SDL_Quit(); return 0;\n}\n', encoding='utf-8')
    objects = shlex.split((BUILD / 'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    response = OUT / 'objects.rsp'
    response.write_text('\n'.join('"' + p + '"' for p in objects if not p.endswith((
        '/src/main.c.obj', '/src/object/object-ui-core.c.obj',
        '/src/object/object-ui-select.c.obj'))), encoding='utf-8')
    env = os.environ.copy()
    env['PATH'] = os.pathsep.join([
        *(str(BUILD / '_deps' / p) for p in ('SDL', 'SDL_ttf', 'SDL_image', 'SDL_mixer')),
        'C:/msys64/mingw64/bin', 'C:/msys64/usr/bin', env['PATH']])
    exe = OUT / 'check.exe'
    # Bounds trapping instruments the two current selector sources without
    # requiring a sanitizer runtime; the pre-fix wide case traps immediately.
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe', '-DUSE_SDL', '-std=c17', '-O0',
        '-fsanitize=bounds', '-fsanitize-undefined-trap-on-error',
        '@CMakeFiles/sil-more.dir/includes_C.rsp', str(source),
        str(ROOT / 'src/object/object-ui-core.c'), str(ROOT / 'src/object/object-ui-select.c'),
        '@' + str(response), '@CMakeFiles/sil-more.dir/linkLibs.rsp',
        '-Wl,--wrap=inkey,--wrap=message_flush,--wrap=handle_stuff,--wrap=sdl_touch_only_device_active',
        '-o', str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-', dir=OUT) as state:
        subprocess.run([str(exe), str(ROOT / 'lib/edit'), state], cwd=state,
                       env=env, check=True, timeout=20)


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Exercise actual Smithing Details routing without spending craft resources.

Uses initialized production templates, previews, pending Reforge state and the
real keyboard/click consumers. UI input and unrelated repaint are fixtures.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
import argparse
from check_monster_scent_save import ENGINE_FIXTURE, fixture_function
from check_reforge_work_transaction import CHECKS as WORK_CHECKS, SMITH

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/smithing-cost-details"
EXTRA = r'''
#include <assert.h>
void fixture_type_shortcut(int expected)
{
    int highlight = 1;
    assert(create_tval_menu_aux(&highlight) == expected);
    assert(highlight == expected);
    int first = smith_ui_dense_row0();
    int last = smith_ui_list_bottom_row(first, false);
    int top = smith_ui_configure_list_view(SMITH_SCROLL_TVAL,
        MAX_SMITHING_TVALS, highlight, first, last);
    int row = first + highlight - 1 - top;
    assert(row >= first && row <= last);
    byte attr; char glyph;
    assert(!Term_what(indexed_menu_prefix_col(COL_SMT2), row, &attr, &glyph));
    assert(attr == smith_ui_selected_attr(TERM_WHITE));
    assert(glyph == 'a' + expected - 1);
}
void fixture_details(int item, int prefix, int mode)
{
    if (mode < 2 || mode == 4)
    {
        reforge_preview_type preview;
        assert(reforge_preview_build(player_inventory_object(item), prefix, &preview));
        prt_reforge_preview(&preview);
        assert(smith_ui_detail_valid && !smith_ui_detail_paid);
        assert(!memcmp(&preview.cost, &smith_ui_detail_cost, sizeof(preview.cost)));
        assert(smith_ui_detail_turns == preview.turns);
    }
    else
    {
        if (mode == 3) object_wipe(smith_o_ptr);
        prt_object_difficulty();
        if (mode == 2)
        {
            assert(smith_ui_detail_valid && smith_ui_detail_paid);
            smithing_cost_type zero = { 0 };
            assert(!memcmp(&zero, &smith_ui_detail_cost, sizeof(zero)));
            assert(smith_ui_detail_turns == p_ptr->smithing_leftover);
        }
        else assert(!smith_ui_detail_valid);
    }

    object_type object = *smith_o_ptr;
    smithing_cost_type cost = smithing_cost;
    smith_alloy_state alloy = smith_alloy;
    int work = p_ptr->smithing, remaining = p_ptr->smithing_leftover;
    if (mode == 1 || mode == 4)
    {
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        smith_ui_begin_touch_scroll_area(false);
        bool found = false;
        for (int i = 0; i < ui_menu_click_touch_button_count(); ++i)
        {
            int choice; cptr label; byte attr;
            assert(ui_menu_click_touch_button_get(i, &choice, &label, &attr));
            if (choice == SMITH_CLICK_CALC && streq(label, "Details")) found = true;
        }
        assert(found);
        bool wake = false;
        int choice, action;
        assert(ui_menu_click_handle_choice_action(SMITH_CLICK_CALC,
            mode == 4 ? UI_MENU_CLICK_HOVER : UI_MENU_CLICK_PRIMARY, &wake));
        assert(smith_ui_take_click_action(&choice, &action));
        assert(choice == 0 && action == UI_MENU_CLICK_HOVER);
    }
    else assert(smithing_menu_key('?') == 0);
    assert(!memcmp(&object, smith_o_ptr, sizeof(object)));
    assert(!memcmp(&cost, &smithing_cost, sizeof(cost)));
    assert(!memcmp(&alloy, &smith_alloy, sizeof(alloy)));
    assert(p_ptr->smithing == work && p_ptr->smithing_leftover == remaining);
}
'''
CHECKS = r'''
#include "sdl-config.h"
extern void fixture_details(int,int,int);
extern void fixture_type_shortcut(int);
static void details_checks(void)
{
    scenario = "read-only Smithing Details";
    prepare(3);
    config.menu_bigger_font[SDL_MENU_FONT_SMITHING] = true;
    sdl_push_terminal_menu_scale_for(SDL_MENU_FONT_SMITHING);
    int prefix, turns, dex;
    assert(fixture_reforge_plan(selected_item, &prefix, &turns, &dex) >= 0);
    for (int mode = 0; mode < 5; ++mode)
    {
        if (mode == 2) assert(fixture_begin(selected_item, prefix));
        long xp = p_ptr->new_exp, world = turn, actions = playerturn;
        int energy = p_ptr->energy_use, uses = forge_uses(5, 5), count = source_count();
        keys = mode == 4 ? "" : "\033";
        fixture_details(selected_item, prefix, mode);
        assert(!*keys);
        assert(p_ptr->new_exp == xp && turn == world && playerturn == actions);
        assert(p_ptr->energy_use == energy && forge_uses(5, 5) == uses);
        assert(source_count() == count);
        if (mode == 3)
        {
            prepare(3);
            assert(fixture_reforge_plan(selected_item, &prefix, &turns, &dex) >= 0);
        }
    }
    puts("Smithing Details: actual preview bill, keyboard/click/hover, paid remaining work, empty blueprint and unchanged resources/turns PASS");
    scenario = "Smithing keyboard type highlight";
    set_sdl_input_ui_mode(SDL_INPUT_UI_MODE_PLATFORM);
    const int heights[] = { 24, 12 };
    for (int h = 0; h < 2; h++)
    {
        if (Term->wid != 80 || Term->hgt != heights[h])
            assert(!Term_resize(80, heights[h]));
        keys = "g"; fixture_type_shortcut(7); assert(!*keys);
        keys = "q"; fixture_type_shortcut(17); assert(!*keys);
    }
    puts("Smithing type shortcuts repaint selected Ring/Boots parent row, including offscreen choices PASS");
    sdl_pop_terminal_menu_scale();
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--type-baseline", action="store_true")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    prefix += fixture_function("terminal_extra") + "\n" + fixture_function("reset_map") + "\n"
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    source = OUT / "check.c"
    source.write_text(prefix + WORK_CHECKS[:WORK_CHECKS.index("static void checks(void)")]
                      + CHECKS + init + "details_checks(); SDL_Quit(); return 0;\n}\n", encoding="utf-8")
    smith = OUT / "smith.c"
    smith_wrapper = SMITH
    if args.type_baseline:
        body = (ROOT / "src/cmd/ui/cmd-ui-smithing.c").read_text(encoding="utf-8")
        old = "if (valid[*highlight - 1])\n            ch = '\\r';"
        assert body.count(old) == 1
        baseline = OUT / "smith-type-baseline.c"
        baseline.write_text(body.replace(old,
            "if (valid[*highlight - 1])\n            return (*highlight);"), encoding="utf-8")
        smith_wrapper = SMITH.replace('"cmd/ui/cmd-ui-smithing.c"',
            '"' + baseline.as_posix() + '"')
    smith.write_text(smith_wrapper + EXTRA, encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    excluded = ("/src/main.c.obj", "/src/cmd/ui/cmd-ui-smithing.c.obj")
    response.write_text("\n".join('"' + path + '"' for path in objects
                                 if not path.endswith(excluded)), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / name) for name in
        ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    wraps = ("inkey", "open_inventory_item_select_menu", "handle_stuff", "msg_print",
             "request_command", "process_command", "get_sdl_config_path",
             "tutorial_game_action_allowed")
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), str(smith), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", *("-Wl,--wrap=" + name for name in wraps),
        "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="fixture-", dir=OUT) as state:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), state], cwd=state,
                       env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()

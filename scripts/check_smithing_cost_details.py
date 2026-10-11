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
from check_monster_scent_save import ENGINE_FIXTURE, fixture_function
from check_reforge_work_transaction import CHECKS as WORK_CHECKS, SMITH

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/smithing-cost-details"
EXTRA = r'''
#include <assert.h>
void fixture_details(int item, int prefix, int mode)
{
    const object_type* source = NULL;
    smithing_cost_type expected_cost = { 0 };
    int expected_turns = p_ptr->smithing_leftover;
    if (mode < 2 || mode == 4)
    {
        reforge_preview_type preview;
        source = player_inventory_object(item);
        assert(reforge_preview_build(source, prefix, &preview));
        prt_reforge_preview(&preview);
        expected_cost = preview.cost;
        expected_turns = preview.turns;
    }
    else
    {
        if (mode == 3) object_wipe(smith_o_ptr);
        prt_object_difficulty();
    }

    object_type object = *smith_o_ptr;
    smithing_cost_type cost = smithing_cost;
    smith_alloy_state alloy = smith_alloy;
    int work = p_ptr->smithing, remaining = p_ptr->smithing_leftover;
    smith_calculation_report report = { 0 }, expected = { 0 };
    smith_build_calculation_report(&report, 76, source);
    if (mode != 3)
    {
        smith_report_costs(&expected, 76, &expected_cost,
            expected_turns, source != NULL);
        assert(report.count >= expected.count);
        for (int i = 0; i < expected.count; ++i)
            assert(streq(report.text[report.count - expected.count + i], expected.text[i]));
        if (mode == 2)
        {
            bool paid = false;
            for (int i = 0; i < report.count; ++i)
                if (strstr(report.text[i], "Costs already paid")) paid = true;
            assert(paid);
        }
    }
    else
    {
        bool empty = false;
        for (int i = 0; i < report.count; ++i)
            if (strstr(report.text[i], "Choose a base item")) empty = true;
        assert(empty);
    }

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
            if (choice == SMITH_CLICK_CALC && streq(label, "How calculated")) found = true;
        }
        assert(found);
        bool wake = false;
        int choice, action;
        assert(ui_menu_click_handle_choice_action(SMITH_CLICK_CALC,
            mode == 4 ? UI_MENU_CLICK_HOVER : UI_MENU_CLICK_PRIMARY, &wake));
        assert(ui_menu_click_take_action(&choice, &action));
        assert(choice == SMITH_CLICK_CALC);
        assert(action == (mode == 4 ? UI_MENU_CLICK_HOVER : UI_MENU_CLICK_PRIMARY));
        if (action != UI_MENU_CLICK_HOVER) smith_show_calculations(source);
    }
    else assert(smithing_menu_key('?', source) == 0);
    assert(!memcmp(&object, smith_o_ptr, sizeof(object)));
    assert(!memcmp(&cost, &smithing_cost, sizeof(cost)));
    assert(!memcmp(&alloy, &smith_alloy, sizeof(alloy)));
    assert(p_ptr->smithing == work && p_ptr->smithing_leftover == remaining);
}
'''
CHECKS = r'''
extern void fixture_details(int,int,int);
bool __wrap_sdl_touch_only_device_active(void) { return true; }
static void details_checks(void)
{
    scenario = "read-only Smithing Details";
    prepare(3);
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
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    prefix += fixture_function("terminal_extra") + "\n" + fixture_function("reset_map") + "\n"
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    source = OUT / "check.c"
    source.write_text(prefix + WORK_CHECKS[:WORK_CHECKS.index("static void checks(void)")]
                      + CHECKS + init + "details_checks(); SDL_Quit(); return 0;\n}\n", encoding="utf-8")
    smith = OUT / "smith.c"
    smith.write_text(SMITH + EXTRA, encoding="utf-8")
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
             "tutorial_game_action_allowed", "sdl_touch_only_device_active")
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), str(smith), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", *("-Wl,--wrap=" + name for name in wraps),
        "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="fixture-", dir=OUT) as state:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), state], cwd=state,
                       env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()

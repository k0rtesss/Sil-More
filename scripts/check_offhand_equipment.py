#!/usr/bin/env python3
"""Check real Off-hand eligibility and browser routing against built engine objects.

--baseline uses the committed browser to prove the omitted candidate regression.
UI confirmation/wield are instrumented; the production legacy/forced-slot policy
is compiled directly. Live GameControl covers actual transfer and save/reload.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
from check_equipment_action_routing import HARNESS

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/offhand-equipment"
CHECKS = r'''
    extern bool fixture_forced_offhand(const object_type*);
    reset_calls(); set_item(0,1);
    p_ptr->active_weapon_mode=PLAYER_ACTIVE_WEAPON_MELEE;
    p_ptr->active_ability[S_MEL][MEL_TWO_WEAPON]=true;
    assert(do_cmd_can_wield_offhand(&held[0]));
    assert(fixture_forced_offhand(&held[0]));
    assert(equipment_slot_accepts_object(INVEN_ARM,&held[0]));
    assert(streq(equipment_menu_use_action_text(&entry,INVEN_ARM,
        SUPPLY_FLOOR_ACTION_DEFAULT),"Equip"));
    assert(equipment_menu_use_entry(&entry,INVEN_ARM,SUPPLY_FLOOR_ACTION_DEFAULT));
    assert(wield_slot_seen==INVEN_ARM && active_calls==0);
    reset_calls(); confirmation=false; p_ptr->energy_use=0;
    object_type saved=held[0];
    assert(!equipment_menu_use_entry(&entry,INVEN_ARM,SUPPLY_FLOOR_ACTION_DEFAULT));
    assert(wield_slot_seen==-1 && !active_calls && !p_ptr->energy_use);
    assert(!memcmp(&saved,&held[0],sizeof(saved)));
    p_ptr->active_ability[S_MEL][MEL_TWO_WEAPON]=false;
    assert(!do_cmd_can_wield_offhand(&held[0]));
    assert(!fixture_forced_offhand(&held[0]));
    assert(!equipment_slot_accepts_object(INVEN_ARM,&held[0]));
    held[0].abilities=1; held[0].skilltype[0]=S_MEL;
    held[0].abilitynum[0]=MEL_TWO_WEAPON;
    assert(!do_cmd_can_wield_offhand(&held[0]));
    object_known(&held[0]);
    assert(do_cmd_can_wield_offhand(&held[0]));
    assert(fixture_forced_offhand(&held[0]));
    p_ptr->have_ability[S_MEL][MEL_TWO_WEAPON]=true;
    assert(!do_cmd_can_wield_offhand(&held[0]));
    assert(!fixture_forced_offhand(&held[0]));
    p_ptr->have_ability[S_MEL][MEL_TWO_WEAPON]=false;
    int weapon_types[]={TV_SWORD,TV_POLEARM,TV_HAFTED,TV_DIGGING};
    for(int i=0;i<4;i++) {
        held[0].tval=kinds[1].tval=weapon_types[i];
        assert(do_cmd_can_wield_offhand(&held[0]));
        assert(fixture_forced_offhand(&held[0]));
        assert(equipment_slot_accepts_object(INVEN_ARM,&held[0]));
    }
    held[0].tval=kinds[1].tval=TV_SWORD;
    kinds[1].flags3|=TR3_HAND_AND_A_HALF;
    assert(!do_cmd_can_wield_offhand(&held[0]));
    assert(!fixture_forced_offhand(&held[0]));
    kinds[1].flags3 &= ~TR3_HAND_AND_A_HALF;
    kinds[1].flags3 |= TR3_TWO_HANDED;
    assert(!equipment_slot_accepts_object(INVEN_ARM,&held[0]));
    set_item(0,3); assert(!do_cmd_can_wield_offhand(&held[0]));
    assert(!fixture_forced_offhand(&held[0]));
    set_item(0,5); assert(equipment_slot_accepts_object(INVEN_ARM,&held[0]));
    assert(fixture_forced_offhand(&held[0]));
    puts("PASS: Off-hand browser/forced policy, known granted ability, handedness, shield, explicit dispatch and free cancellation.");
    return 0;
}
'''

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", action="store_true")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    source_text = HARNESS[:HARNESS.index("    reset_calls();\n    set_item(0, 1);")] + CHECKS
    source_text = source_text.replace("#include <assert.h>", "#include <assert.h>\n#undef assert\n#define assert(c) do { if (!(c)) { fprintf(stderr, \"FAIL %d: %s\\n\", __LINE__, #c); exit(1); } } while (0)")
    if args.baseline:
        browser = OUT / "browser-baseline.c"
        browser.write_bytes(subprocess.check_output(["git", "-c", f"safe.directory={ROOT.as_posix()}",
            "show", "HEAD:src/cmd/ui/cmd-ui-knowledge.c"], cwd=ROOT))
        source_text = source_text.replace('"cmd/ui/cmd-ui-knowledge.c"', '"' + browser.as_posix() + '"')
    source = OUT / "check.c"
    source.write_text(source_text, encoding="utf-8")
    core = OUT / "core.c"
    core.write_text('#include "cmd/item/cmd-item-core.c"\n'
        'bool fixture_forced_offhand(const object_type* object) {\n'
        ' return forced_wield_slot_accepts_object(object,INVEN_ARM); }\n', encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/cmd/ui/cmd-ui-knowledge.c.obj", "/src/cmd/item/cmd-item-core.c.obj",
        "/src/player/player-active-weapon.c.obj")
    rsp = OUT / "objects.rsp"
    rsp.write_text("\n".join('"' + p + '"' for p in objects if not p.endswith(excluded)))
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([*[str(BUILD / "_deps" / x) for x in
        ("SDL", "SDL_image", "SDL_ttf", "SDL_mixer")], "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    wrapped = ("do_cmd_toggle_active_weapon", "do_cmd_wield_to_slot", "do_cmd_move_item_to_storage",
        "do_cmd_use_item_by_index", "get_check", "player_pack_item_action_blocked", "object_desc",
        "ui_question_ask_objects_with_help", "do_cmd_active_item", "calc_bonuses_for_preview", "update_stuff")
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), str(core),
        str(ROOT / "src/player/player-active-weapon.c"), "@" + str(rsp),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", *["-Wl,--wrap=" + s for s in wrapped],
        "-o", str(exe)], cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe)], cwd=ROOT, env=env, check=True, timeout=30)

if __name__ == "__main__":
    main()

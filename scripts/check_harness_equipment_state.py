#!/usr/bin/env python3
"""Check cursed Harness transfers and identification with production engine code.

Requires an incremental Windows build. --baseline compiles the committed item
commands and browser to demonstrate the removal and examination regressions.
All templates and runtime files are loaded into a temporary fixture directory.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile

from check_new_monsters import HARNESS as ENGINE_FIXTURE
from check_monster_scent_save import fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/harness-equipment-state"

CHECKS = r'''
#include "supplies.h"
#include "ui/question.h"
extern void player_pack_action_process(void);
extern bool fixture_examine(int item, bool supplies);

static int examined, examined_item;
static bool examined_equipped;
static int knowledge_checks[INVEN_TOTAL];
static bool capturing_knowledge;
static bool transfer_identified_equipped;
static bool pack_choice_seen, pack_choice_disabled;

void __wrap_handle_stuff(void) {}
void __wrap_update_stuff(void) {}
void __wrap_calc_bonuses_for_preview(void) {}
void __wrap_msg_print(cptr message) { (void)message; }
void __wrap_msg_format(cptr format, ...) { (void)format; }
bool __wrap_get_check(cptr prompt) { (void)prompt; return true; }
s16b __wrap_get_quantity_action(cptr prompt, cptr action, int max)
{ (void)prompt; (void)action; return max; }
bool __wrap_tutorial_game_action_allowed(const char* action, const object_type* obj)
{ (void)action; (void)obj; return true; }
void __wrap_tutorial_game_explain(const char* id, const char* title, const char* text)
{ (void)id; (void)title; (void)text; }
void __wrap_object_info_screen(const object_type* obj) { (void)obj; }
void __wrap_object_info_screen_multi(const object_type** objs,
    const char** headings, int count, bool compare)
{ (void)objs; (void)headings; (void)count; (void)compare; }
bool __wrap_player_try_identify_smithing_object_on_examine(
    object_type* obj, bool equipped)
{
    examined++;
    examined_item = player_inventory_handle_for_object(obj);
    examined_equipped = equipped;
    return false;
}
void __wrap_self_knowledge(void) {}
int __wrap_ui_question_ask_objects_with_help(cptr title, cptr desc,
    const ui_question_option* options, const object_type* const icons[],
    int count, int y, int x, int initial)
{
    (void)title; (void)desc; (void)icons; (void)y; (void)x; (void)initial;
    for (int i = 0; i < count; i++)
        if (strstr(options[i].label, "Store in Pack"))
        {
            pack_choice_seen = true;
            pack_choice_disabled = options[i].disabled;
        }
    return -1;
}
bool __wrap_player_try_identify_smithing_object(object_type* obj,
    bool equipped, int bonus)
{
    if (!capturing_knowledge)
    {
        transfer_identified_equipped = equipped;
        return false;
    }
    assert(equipped && bonus == 5);
    for (int i = 0; i < INVEN_TOTAL; i++)
        if (obj == &inventory[i]) knowledge_checks[i]++;
    return false;
}

static void make_item(int slot, int tval, int sval)
{
    int kind = lookup_kind(tval, sval);
    assert(kind > 0);
    object_prep(&inventory[slot], kind);
    inventory[slot].number = 1;
    inventory[slot].storage = OBJECT_STORAGE_HARNESS;
    inventory[slot].pickup_slot = -1;
}

static void prepare(int slot, bool active)
{
    reset_map(1);
    memset(inventory, 0, INVEN_TOTAL * sizeof(*inventory));
    player_carried_extra_reset_store();
    player_quiver_reset_store();
    supplies_reset_store();
    player_pack_action_reset();
    memset(p_ptr->active_ability, 0, sizeof(p_ptr->active_ability));
    memset(p_ptr->have_ability, 0, sizeof(p_ptr->have_ability));
    make_item(INVEN_WIELD, TV_SWORD, SV_SHORT_SWORD);
    make_item(INVEN_BOW, TV_BOW, SV_LONG_BOW);
    if (slot == INVEN_ARM) make_item(slot, TV_SHIELD, SV_ROUND_SHIELD);
    if (slot == INVEN_WIELD) make_item(slot, TV_SWORD, SV_DAGGER);
    p_ptr->active_weapon_mode = ((slot == INVEN_BOW) == active)
        ? PLAYER_ACTIVE_WEAPON_RANGED_1 : PLAYER_ACTIVE_WEAPON_MELEE;
    p_ptr->stat_use[A_STR] = 5;
    p_ptr->playing = true;
    p_ptr->leaving = p_ptr->is_dead = false;
    p_ptr->inven_cnt = 0;
    p_ptr->equip_cnt = slot == INVEN_ARM ? 3 : 2;
    p_ptr->energy_use = 0;
    p_ptr->total_weight = 0;
    for (int i = 0; i < INVEN_TOTAL; i++)
        p_ptr->total_weight += inventory[i].weight * inventory[i].number;
    character_generated = true;
    character_xtra = false;
    character_icky = 0;
    inventory[slot].ident |= IDENT_CURSED;
    assert(player_equipment_slot_counts_as_equipped(slot) == active);
}

static object_type* carried_kind(int kind)
{
    for (int i = 0; i < player_pack_entry_count(); i++)
    {
        object_type* obj = player_pack_entry_at(i);
        if (obj->k_idx == kind) return obj;
    }
    return NULL;
}

static void finish_pack(void)
{
    for (int i = 0; i < 3 && player_pack_action_pending(); i++)
        player_pack_action_process();
    assert(!player_pack_action_pending());
}

static void check_removal(void)
{
    int slots[] = { INVEN_WIELD, INVEN_BOW, INVEN_ARM };
    for (int i = 0; i < 3; i++)
        for (int breaking = 0; breaking < 2; breaking++)
        {
            int slot = slots[i];
            prepare(slot, false);
            int kind = inventory[slot].k_idx;
            p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING] = breaking;
            do_cmd_takeoff(&inventory[slot], slot);
            assert(!inventory[slot].k_idx);
            object_type* moved = carried_kind(kind);
            assert(moved && cursed_p(moved) && !(moved->ident & IDENT_UNCURSED));

            prepare(slot, false);
            kind = inventory[slot].k_idx;
            p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING] = breaking;
            assert(do_cmd_drop_item_by_index_confirm(slot, false));
            assert(!inventory[slot].k_idx);
            bool found = false;
            for (int j = 1; j < o_max; j++)
                if (o_list[j].k_idx == kind)
                {
                    assert(cursed_p(&o_list[j]) && !(o_list[j].ident & IDENT_UNCURSED));
                    found = true;
                }
            assert(found);
        }
    for (int i = 0; i < 3; i++)
    {
        int slot = slots[i];
        prepare(slot, true);
        int kind = inventory[slot].k_idx;
        do_cmd_takeoff(&inventory[slot], slot);
        assert(inventory[slot].k_idx == kind && cursed_p(&inventory[slot]));
        assert(!do_cmd_drop_item_by_index_confirm(slot, false));
        assert(p_ptr->energy_use == 0 && !player_pack_action_pending());
    }
    puts("PASS: Stored cursed melee weapons, bows and shields can be removed/dropped, preserve their curse even with Curse Breaking; equipped controls remain bound.");
}

static void check_transfers(void)
{
    prepare(INVEN_WIELD, false);
    int kind = inventory[INVEN_WIELD].k_idx;
    assert(do_cmd_move_item_to_storage(INVEN_WIELD, OBJECT_STORAGE_PACK));
    assert(player_pack_action_pending());
    finish_pack();
    object_type* moved = carried_kind(kind);
    assert(!inventory[INVEN_WIELD].k_idx && moved && cursed_p(moved));
    assert(moved->storage == OBJECT_STORAGE_PACK);
    prepare(INVEN_WIELD, true);
    assert(!do_cmd_move_item_to_storage(INVEN_WIELD, OBJECT_STORAGE_PACK));
    assert(!player_pack_action_pending() && cursed_p(&inventory[INVEN_WIELD]));

    for (int active = 0; active < 2; active++)
    {
        prepare(INVEN_WIELD, active);
        pack_choice_seen = false;
        assert(!do_cmd_active_item(INVEN_WIELD));
        assert(pack_choice_seen && pack_choice_disabled == active);

        make_item(0, TV_SWORD, SV_DAGGER);
        inventory[0].storage = OBJECT_STORAGE_PACK;
        p_ptr->inven_cnt = 1;
        p_ptr->total_weight += inventory[0].weight;
        assert(do_cmd_move_item_to_storage_exchange(0, OBJECT_STORAGE_HARNESS,
            INVEN_WIELD, 1) == !active);
        if (!active)
        {
            finish_pack();
            int harness_count = 0, pack_count = 0;
            for (int i = 0; i < player_pack_entry_count(); i++)
            {
                moved = player_pack_entry_at(i);
                if (moved->tval != TV_SWORD || moved->sval != SV_DAGGER) continue;
                if (moved->storage == OBJECT_STORAGE_HARNESS && !cursed_p(moved))
                    harness_count += moved->number;
                if (moved->storage == OBJECT_STORAGE_PACK && cursed_p(moved))
                    pack_count += moved->number;
            }
            assert(harness_count == 1 && pack_count == 1);
        }

        prepare(INVEN_WIELD, active);
        kind = inventory[INVEN_WIELD].k_idx;
        make_item(0, TV_SWORD, SV_SHORT_SWORD);
        p_ptr->inven_cnt = 1;
        p_ptr->total_weight += inventory[0].weight;
        assert(do_cmd_wield_to_slot(&inventory[0], 0, INVEN_WIELD) == !active);
        if (active)
            assert(inventory[INVEN_WIELD].k_idx == kind);
        else
        {
            assert(inventory[INVEN_WIELD].sval == SV_SHORT_SWORD);
            assert(!transfer_identified_equipped);
            moved = carried_kind(kind);
            assert(moved && cursed_p(moved));
        }
    }
    puts("PASS: Stored cursed gear enables Pack storage, moves/exchanges to Pack and can be displaced; equipped cursed gear remains bound.");
}

static void check_identification(void)
{
    int slots[] = { INVEN_WIELD, INVEN_BOW, INVEN_ARM };
    for (int i = 0; i < 3; i++)
        for (int active = 0; active < 2; active++)
            for (int supplies = 0; supplies < 2; supplies++)
            {
                prepare(slots[i], active);
                examined = 0;
                assert(fixture_examine(slots[i], supplies));
                assert(examined == 1 && examined_item == slots[i]);
                assert(examined_equipped == active);
            }
    prepare(INVEN_WIELD, false);
    make_item(0, TV_SWORD, SV_DAGGER);
    for (int supplies = 0; supplies < 2; supplies++)
    {
        examined = 0;
        assert(fixture_examine(0, supplies));
        assert(examined == 1 && examined_item == 0 && !examined_equipped);
    }
    object_type extra = inventory[0];
    assert(player_carried_extra_load(&extra));
    for (int supplies = 0; supplies < 2; supplies++)
    {
        examined = 0;
        assert(fixture_examine(CARRIED_EXTRA_INDEX, supplies));
        assert(examined == 1 && examined_item == CARRIED_EXTRA_INDEX && !examined_equipped);
    }
    puts("PASS: Both examination menus always attempt identification; stored/expandable Harness entries get carried context and active gear gets equipped context.");
}

static void check_self_knowledge(void)
{
    int slots[] = { INVEN_WIELD, INVEN_BOW, INVEN_ARM };
    for (int i = 0; i < 3; i++)
        for (int active = 0; active < 2; active++)
        {
            prepare(slots[i], active);
            memset(knowledge_checks, 0, sizeof(knowledge_checks));
            object_type gem;
            object_prep(&gem, lookup_kind(TV_GEM, SV_GEM_SELF_KNOWLEDGE));
            bool identified = false;
            capturing_knowledge = true;
            assert(use_object(&gem, &identified) && identified);
            capturing_knowledge = false;
            assert(knowledge_checks[slots[i]] == active);
        }
    puts("PASS: Self Knowledge identification checks include active gear and exclude stored Harness gear.");
}
'''

WRAPPER = r'''
bool fixture_examine(int item, bool supplies)
{
    if (supplies)
    {
        supply_list_entry entry = {0};
        entry.equip_idx = player_inventory_handle_is_equipped(item) ? item : -1;
        entry.item_idx = item;
        entry.supply_idx = entry.k_idx = -1;
        entry.preset_idx = entry.floor_idx = -1;
        return supplies_menu_show_entry_description(&entry, false);
    }
    equipment_list_entry entry;
    equipment_entry_clear(&entry);
    entry.equip_idx = player_inventory_handle_is_equipped(item) ? item : -1;
    entry.item_idx = item;
    return equipment_menu_show_entry_description(&entry, INVEN_WIELD, false);
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", action="store_true")
    parser.add_argument("--case", choices=("all", "removal", "transfers", "identification", "self_knowledge"), default="all")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    prefix += fixture_function("terminal_extra") + "\n" + fixture_function("reset_map") + "\n"
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    cases = ("removal", "transfers", "identification", "self_knowledge") if args.case == "all" else (args.case,)
    source = OUT / "check.c"
    source.write_text(prefix + CHECKS + init
        + "    assert(!init_flavor_info()); flavor_init();\n"
        + "".join(f"    check_{case}();\n" for case in cases)
        + "    SDL_Quit(); return 0;\n}\n", encoding="utf-8")
    ui_source = ROOT / "src/cmd/ui/cmd-ui-knowledge.c"
    excluded = ["/src/main.c.obj", "/src/cmd/ui/cmd-ui-knowledge.c.obj"]
    extra = []
    if args.baseline:
        for unit, name in (("cmd/item/cmd-item-core", "core"), ("cmd/ui/cmd-ui-knowledge", "ui")):
            baseline = OUT / f"baseline-{name}.c"
            baseline.write_bytes(subprocess.check_output(["git", "show", f"HEAD:src/{unit}.c"], cwd=ROOT))
            if name == "ui":
                ui_source = baseline
            else:
                extra.append(str(baseline))
                excluded.append(f"/src/{unit}.c.obj")
    wrapper = OUT / "ui-check.c"
    wrapper.write_text(f'#include "{ui_source.as_posix()}"\n' + WRAPPER, encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"'+p+'"' for p in objects if not p.endswith(tuple(excluded))))
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / dep) for dep in
        ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")), "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    wrapped = ("handle_stuff", "update_stuff", "calc_bonuses_for_preview",
        "msg_print", "msg_format", "get_check", "ui_question_ask_objects_with_help",
        "get_quantity_action", "tutorial_game_action_allowed", "tutorial_game_explain",
        "object_info_screen", "object_info_screen_multi",
        "player_try_identify_smithing_object_on_examine", "self_knowledge",
        "player_try_identify_smithing_object")
    exe = OUT / ("baseline.exe" if args.baseline else "check.exe")
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", "-I"+str(ROOT / "src/cmd/ui"),
        str(source), str(wrapper), *extra, "@"+str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", *("-Wl,--wrap="+s for s in wrapped), "-o", str(exe)],
        cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="fixture-", dir=OUT) as state:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), state],
            cwd=state, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()

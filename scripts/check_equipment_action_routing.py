#!/usr/bin/env python3
"""Exercise production inventory actions and Quick Throw using built SDL objects."""
from pathlib import Path
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/equipment-action-routing"

HARNESS = r'''
#include "cmd/ui/cmd-ui-knowledge.c"
#include <assert.h>

static object_type held[INVEN_TOTAL];
static object_kind kinds[5];
static maxima limits;
static int active_calls, wield_slot_seen, storage_seen, use_calls;
static int question_result;
static bool confirmation;

void __wrap_do_cmd_toggle_active_weapon(void) { active_calls++; }
bool __wrap_do_cmd_wield_to_slot(object_type* obj, int item, int slot)
{ assert(obj == &held[item]); wield_slot_seen = slot; return true; }
bool __wrap_do_cmd_move_item_to_storage(int item, byte storage)
{ assert(item == 0); storage_seen = storage; return true; }
void __wrap_do_cmd_use_item_by_index(int item)
{ assert(item == 0); use_calls++; }
bool __wrap_get_check(cptr prompt) { (void)prompt; return confirmation; }
bool __wrap_player_pack_item_action_blocked(const object_type* obj)
{ (void)obj; return false; }
void __wrap_object_desc(char* buf, size_t size, const object_type* obj,
    int pref, int mode)
{ (void)obj; (void)pref; (void)mode; SDL_strlcpy(buf, "test item", size); }
int __wrap_ui_question_ask(cptr title, cptr desc,
    const ui_question_option* options, int count, int y, int x, int def)
{
    (void)title; (void)desc; (void)y; (void)x; (void)def;
    assert(count == 2);
    assert(streq(options[0].label, "Store in Pack"));
    assert(streq(options[1].label, "Equip on Belt"));
    return question_result;
}

static void reset_calls(void)
{
    active_calls = use_calls = 0;
    wield_slot_seen = storage_seen = -1;
    confirmation = true;
    question_result = -1;
}

static void set_item(int slot, int kind)
{
    memset(&held[slot], 0, sizeof(held[slot]));
    held[slot].k_idx = kind;
    held[slot].tval = kinds[kind].tval;
    held[slot].sval = kinds[kind].sval;
    held[slot].number = 1;
    held[slot].storage = OBJECT_STORAGE_HARNESS;
    held[slot].volume = 4;
    held[slot].pickup_slot = -1;
}

int main(void)
{
    inventory = held;
    k_info = kinds;
    z_info = &limits;
    kinds[1].tval = TV_SWORD;
    kinds[1].sval = SV_DAGGER;
    kinds[1].flags3 = TR3_THROWING;
    kinds[1].flags4 = TR4_HARNESS_STOWABLE;
    kinds[2].tval = TV_DIGGING;
    kinds[2].flags3 = TR3_TWO_HANDED;
    kinds[2].flags4 = TR4_HARNESS_STOWABLE;
    kinds[3].tval = TV_BOW;
    kinds[3].sval = SV_SHORT_BOW;
    kinds[4].tval = TV_ARROW;
    p_ptr->inven_cnt = 1;
    equipment_list_entry entry;
    equipment_entry_clear(&entry);
    entry.item_idx = 0;
    char failure[240] = "";

    reset_calls();
    set_item(0, 1);
    assert(equipment_menu_use_entry(&entry, INVEN_BELT,
        SUPPLY_FLOOR_ACTION_DEFAULT));
    assert(wield_slot_seen == INVEN_BELT && active_calls == 0);
    reset_calls();
    confirmation = false;
    assert(!equipment_menu_use_entry(&entry, INVEN_BELT,
        SUPPLY_FLOOR_ACTION_DEFAULT));
    assert(wield_slot_seen == -1 && active_calls == 0);
    puts("PASS: Belt equip respects the selected slot and confirmation cancellation.");

    reset_calls();
    assert(equipment_menu_use_entry(&entry, INVEN_WIELD,
        SUPPLY_FLOOR_ACTION_DEFAULT));
    assert(active_calls == 1 && wield_slot_seen == -1);
    reset_calls();
    set_item(0, 3);
    assert(equipment_menu_use_entry(&entry, INVEN_BOW,
        SUPPLY_FLOOR_ACTION_DEFAULT));
    assert(active_calls == 1 && wield_slot_seen == -1);
    assert(streq(inventory_page_use_action_text(&entry,
        SUPPLY_FLOOR_ACTION_DEFAULT), "Equip"));
    assert(inventory_page_use_entry(&entry, SUPPLY_FLOOR_ACTION_DEFAULT,
        failure, sizeof(failure)));
    assert(active_calls == 2);
    puts("PASS: Main-hand and bow Equip continue to open Active setup.");

    reset_calls();
    set_item(0, 1);
    assert(streq(inventory_page_use_action_text(&entry,
        SUPPLY_FLOOR_ACTION_DEFAULT), "Choose"));
    assert(!inventory_page_use_entry(&entry, SUPPLY_FLOOR_ACTION_DEFAULT,
        failure, sizeof(failure)));
    assert(wield_slot_seen == -1 && storage_seen == -1 && active_calls == 0);
    question_result = 0;
    assert(inventory_page_use_entry(&entry, SUPPLY_FLOOR_ACTION_DEFAULT,
        failure, sizeof(failure)));
    assert(storage_seen == OBJECT_STORAGE_PACK);
    reset_calls();
    question_result = 1;
    assert(inventory_page_use_entry(&entry, SUPPLY_FLOOR_ACTION_DEFAULT,
        failure, sizeof(failure)));
    assert(wield_slot_seen == INVEN_BELT && active_calls == 0);
    puts("PASS: Harness dagger supports Pack, Belt, and cancel without unintended equip.");

    reset_calls();
    set_item(INVEN_BELT, 1);
    held[INVEN_BELT].ident |= IDENT_CURSED;
    assert(streq(inventory_page_use_action_text(&entry,
        SUPPLY_FLOOR_ACTION_DEFAULT), "Store"));
    assert(inventory_page_use_entry(&entry, SUPPLY_FLOOR_ACTION_DEFAULT,
        failure, sizeof(failure)));
    assert(storage_seen == OBJECT_STORAGE_PACK && active_calls == 0);
    reset_calls();
    set_item(0, 2);
    assert(streq(inventory_page_use_action_text(&entry,
        SUPPLY_FLOOR_ACTION_DEFAULT), "Store"));
    assert(inventory_page_use_entry(&entry, SUPPLY_FLOOR_ACTION_DEFAULT,
        failure, sizeof(failure)));
    assert(storage_seen == OBJECT_STORAGE_PACK && active_calls == 0);
    held[0].storage = OBJECT_STORAGE_PACK;
    assert(streq(inventory_page_use_action_text(&entry,
        SUPPLY_FLOOR_ACTION_DEFAULT), "Ready"));
    assert(inventory_page_use_entry(&entry, SUPPLY_FLOOR_ACTION_DEFAULT,
        failure, sizeof(failure)));
    assert(storage_seen == OBJECT_STORAGE_HARNESS);
    puts("PASS: Cursed Belt and stowable digging tools retain storage transfers.");

    memset(held, 0, sizeof(held));
    set_item(0, 1);
    p_ptr->active_ability[S_MEL][MEL_THROWING] = true;
    p_ptr->active_weapon_mode = PLAYER_ACTIVE_WEAPON_MELEE;
    assert(player_can_quick_throw_from_harness(0));
    set_item(INVEN_WIELD, 2);
    assert(!player_can_quick_throw_from_harness(0));
    set_item(INVEN_WIELD, 1);
    assert(player_can_quick_throw_from_harness(0));
    p_ptr->active_weapon_mode = PLAYER_ACTIVE_WEAPON_RANGED_1;
    set_item(INVEN_BOW, 3);
    assert(player_can_quick_throw_from_harness(0));
    held[INVEN_BOW].sval = SV_LONG_BOW;
    assert(!player_can_quick_throw_from_harness(0));
    held[INVEN_WIELD].pickup_slot = PICKUP_SLOT_ACTIVE_THROWING;
    assert(player_can_quick_throw_from_harness(0));
    p_ptr->active_ability[S_MEL][MEL_THROWING] = false;
    assert(!player_can_quick_throw_from_harness(0));
    puts("PASS: Quick Throw covers empty hand, melee handedness, Shortbow, Longbow, throwing and ability gating.");
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/cmd/ui/cmd-ui-knowledge.c.obj")
    rsp = OUT / "objects.rsp"
    rsp.write_text("\n".join('"' + p + '"' for p in objects
                             if not p.endswith(excluded)), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *[str(BUILD / "_deps" / x) for x in ("SDL", "SDL_image", "SDL_ttf", "SDL_mixer")],
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    wrapped = ("do_cmd_toggle_active_weapon", "do_cmd_wield_to_slot",
               "do_cmd_move_item_to_storage", "do_cmd_use_item_by_index",
               "get_check", "player_pack_item_action_blocked", "object_desc",
               "ui_question_ask")
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17",
        "-O0", "-g", "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
        "@" + str(rsp), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
        *["-Wl,--wrap=" + s for s in wrapped], "-o", str(exe)],
        cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe)], cwd=ROOT, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()

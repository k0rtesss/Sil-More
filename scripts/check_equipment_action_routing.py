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
static object_kind kinds[6];
static maxima limits;
static int active_calls, wield_slot_seen, storage_seen, use_calls;
static int item_seen, menu_result = -1, expected_icon;
static int combat_rows, pack_rows, belt_rows, harness_rows, combo_rows;
static bool select_combo, simulate_wield;
static bool pack_disabled, belt_disabled;
static character_profile characters[1];
static player_race race;
static bool confirmation;
extern bool test_choose_active_item(int item, int* chosen_item, int* action,
    int* shield_item);
extern bool test_apply_active_item(int item);
void __wrap_calc_bonuses_for_preview(void) {}
void __wrap_update_stuff(void) {}
bool __wrap_do_cmd_active_item(int item)
{ active_calls++; item_seen = item; return confirmation; }

void __wrap_do_cmd_toggle_active_weapon(void) { active_calls++; }
bool __wrap_do_cmd_wield_to_slot(object_type* obj, int item, int slot)
{
    assert(obj == &held[item]);
    wield_slot_seen = slot;
    if (simulate_wield) {
        object_copy(&held[slot], obj);
        object_wipe(obj);
        p_ptr->energy_use = 100;
    }
    return true;
}
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
int __wrap_ui_question_ask_objects_with_help(cptr title, cptr desc,
    const ui_question_option* options, const object_type* const icons[],
    int count, int y, int x, int def)
{
    (void)title; (void)desc; (void)y; (void)x; (void)def;
    combat_rows = pack_rows = belt_rows = harness_rows = combo_rows = 0;
    pack_disabled = belt_disabled = false;
    int combo_index = -1;
    for (int i = 0; i < count; i++) {
        if (strstr(options[i].label, "Store in Pack")) {
            pack_rows++; pack_disabled = options[i].disabled;
        } else if (strstr(options[i].label, "Equip on Belt")) {
            belt_rows++; belt_disabled = options[i].disabled;
        } else if (strstr(options[i].label, "Harness")) {
            harness_rows++;
        } else {
            combat_rows++;
            assert(icons[i] && icons[i]->k_idx == expected_icon);
            if (strstr(options[i].label, " + ")) {
                combo_rows++;
                if (combo_index < 0) combo_index = i;
            }
        }
    }
    return select_combo ? combo_index : menu_result;
}

static void reset_calls(void)
{
    active_calls = use_calls = 0;
    wield_slot_seen = storage_seen = -1;
    confirmation = true;
    item_seen = -1;
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
    setvbuf(stdout, NULL, _IONBF, 0);
    inventory = held;
    k_info = kinds;
    z_info = &limits;
    c_info = characters;
    current_character_profile = characters;
    rp_ptr = &race;
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
    kinds[5].tval = TV_SHIELD;
    kinds[5].sval = SV_ROUND_SHIELD;
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
    assert(active_calls == 1 && wield_slot_seen == -1 && item_seen == 0);
    reset_calls();
    set_item(0, 3);
    assert(equipment_menu_use_entry(&entry, INVEN_BOW,
        SUPPLY_FLOOR_ACTION_DEFAULT));
    assert(active_calls == 1 && wield_slot_seen == -1 && item_seen == 0);
    assert(streq(inventory_page_use_action_text(&entry,
        SUPPLY_FLOOR_ACTION_DEFAULT), "Make active"));
    assert(streq(equipment_menu_use_action_text(&entry, INVEN_BOW,
        SUPPLY_FLOOR_ACTION_DEFAULT), "Make active"));
    assert(inventory_page_use_entry(&entry, SUPPLY_FLOOR_ACTION_DEFAULT,
        failure, sizeof(failure)));
    assert(active_calls == 2);
    puts("PASS: Main-hand and bow Make active open the focused setup menu.");

    /* Same spear/dagger in a reserved slot, normal carried slot, or extra
     * carried storage always opens the same item-specific menu. */
    set_item(INVEN_WIELD, 1);
    p_ptr->active_weapon_mode = PLAYER_ACTIVE_WEAPON_RANGED_1;
    equipment_list_entry reserved;
    equipment_entry_clear(&reserved);
    reserved.equip_idx = INVEN_WIELD;
    assert(streq(inventory_page_use_action_text(&reserved,
        SUPPLY_FLOOR_ACTION_DEFAULT), "Make active"));
    reset_calls();
    assert(inventory_page_use_entry(&reserved, SUPPLY_FLOOR_ACTION_DEFAULT,
        failure, sizeof(failure)));
    assert(active_calls == 1 && item_seen == INVEN_WIELD);
    reset_calls();
    assert(equipment_menu_use_entry(&reserved, INVEN_WIELD,
        SUPPLY_FLOOR_ACTION_DEFAULT));
    assert(active_calls == 1 && item_seen == INVEN_WIELD);
    reset_calls();
    confirmation = false;
    assert(!inventory_page_use_entry(&reserved, SUPPLY_FLOOR_ACTION_DEFAULT,
        failure, sizeof(failure)));
    assert(p_ptr->active_weapon_mode == PLAYER_ACTIVE_WEAPON_RANGED_1);
    char where[80];
    assert(streq(equipment_entry_source_text(&reserved, where, sizeof(where)), "Harness"));
    p_ptr->active_weapon_mode = PLAYER_ACTIVE_WEAPON_MELEE;
    reserved.equipped = true;
    assert(streq(equipment_entry_source_text(&reserved, where, sizeof(where)), "Active"));
    assert(streq(inventory_page_use_action_text(&reserved,
        SUPPLY_FLOOR_ACTION_DEFAULT), "Change setup"));
    assert(streq(equipment_menu_use_action_text(&reserved, INVEN_WIELD,
        SUPPLY_FLOOR_ACTION_DEFAULT), "Change setup"));
    puts("PASS: Reserved weapons use the same menu as loose Harness weapons; cancellation preserves active mode.");

    reset_calls();
    set_item(0, 1);
    assert(player_carried_extra_load(&held[0]));
    entry.item_idx = CARRIED_EXTRA_INDEX;
    assert(inventory_page_use_entry(&entry, SUPPLY_FLOOR_ACTION_DEFAULT,
        failure, sizeof(failure)));
    assert(item_seen == CARRIED_EXTRA_INDEX);
    entry.item_idx = 0;
    player_carried_extra_reset_store();
    held[0].storage = OBJECT_STORAGE_PACK;
    assert(streq(inventory_page_use_action_text(&entry,
        SUPPLY_FLOOR_ACTION_DEFAULT), "Move to Harness"));
    assert(inventory_page_use_entry(&entry, SUPPLY_FLOOR_ACTION_DEFAULT,
        failure, sizeof(failure)));
    assert(storage_seen == OBJECT_STORAGE_HARNESS);
    puts("PASS: Extra storage uses the same route; Pack transfer explicitly says Move to Harness.");

    set_item(0, 1);
    set_item(INVEN_WIELD, 1);
    set_item(INVEN_BOW, 3);
    p_ptr->active_weapon_mode = PLAYER_ACTIVE_WEAPON_RANGED_1;
    expected_icon = 1;
    int chosen_item, chosen_action, chosen_shield;
    object_type snapshot[INVEN_TOTAL];
    player_active_weapon_assign_harness_color(&held[0]);
    player_active_weapon_assign_harness_color(&held[INVEN_WIELD]);
    player_active_weapon_assign_harness_color(&held[INVEN_BOW]);
    memcpy(snapshot, held, sizeof(snapshot));
    menu_result = -1;
    assert(!test_choose_active_item(INVEN_WIELD, &chosen_item,
        &chosen_action, &chosen_shield));
    assert(combat_rows >= 1 && pack_rows == 1 && belt_rows == 1 && harness_rows == 1);
    int reserved_rows = combat_rows;
    assert(memcmp(snapshot, held, sizeof(snapshot)) == 0);
    assert(!test_choose_active_item(0, &chosen_item, &chosen_action,
        &chosen_shield));
    assert(combat_rows == reserved_rows && pack_rows == 1 && belt_rows == 1);
    menu_result = 0;
    assert(test_choose_active_item(0, &chosen_item, &chosen_action,
        &chosen_shield));
    assert(chosen_item == 0 && chosen_action == 0);
    assert(test_choose_active_item(INVEN_WIELD, &chosen_item, &chosen_action,
        &chosen_shield));
    assert(chosen_item == INVEN_WIELD && chosen_action == 0);
    /* The focused weapon menu must expose the same atomic setup as the
     * global Active menu: a Harness weapon plus a Harness shield. */
    set_item(1, 5);
    p_ptr->inven_cnt = 2;
    select_combo = true;
    assert(test_choose_active_item(0, &chosen_item, &chosen_action,
        &chosen_shield));
    assert(combo_rows >= 1 && chosen_item == 0 && chosen_action == 0
        && chosen_shield == 1);
    assert(test_choose_active_item(1, &chosen_item, &chosen_action,
        &chosen_shield));
    assert(combo_rows >= 1 && chosen_action == 0 && chosen_shield == 1);
    object_wipe(&held[INVEN_WIELD]);
    simulate_wield = true;
    p_ptr->active_weapon_mode = PLAYER_ACTIVE_WEAPON_RANGED_1;
    p_ptr->energy_use = 0;
    assert(test_apply_active_item(0));
    assert(held[INVEN_WIELD].k_idx == 1 && held[INVEN_ARM].k_idx == 5);
    assert(p_ptr->active_weapon_mode == PLAYER_ACTIVE_WEAPON_MELEE);
    assert(p_ptr->energy_use == 100);
    simulate_wield = false;
    select_combo = false;
    memset(&held[1], 0, sizeof(held[1]));
    p_ptr->inven_cnt = 1;
    set_item(0, 1);
    set_item(INVEN_WIELD, 1);
    object_wipe(&held[INVEN_ARM]);
    set_item(INVEN_BELT, 1);
    held[INVEN_BELT].ident |= IDENT_CURSED;
    assert(test_choose_active_item(0, &chosen_item, &chosen_action,
        &chosen_shield));
    assert(belt_disabled && !pack_disabled);
    held[INVEN_WIELD].ident |= IDENT_CURSED;
    assert(test_choose_active_item(INVEN_WIELD, &chosen_item, &chosen_action,
        &chosen_shield));
    assert(pack_disabled);
    expected_icon = 3;
    assert(test_choose_active_item(INVEN_BOW, &chosen_item, &chosen_action,
        &chosen_shield));
    assert(chosen_item == INVEN_BOW && combat_rows >= 1 && !pack_rows && !belt_rows);
    puts("PASS: Item menu filters unrelated weapons, offers one-turn weapon + shield setups, and preserves storage routes.");

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
    wrapper = OUT / "active-check.c"
    wrapper.write_text('#include "player/player-active-weapon.c"\n'
        'bool test_choose_active_item(int item, int* chosen_item, int* action, int* shield_item) {\n'
        'active_weapon_choice choice; if (!choose_active_weapon(&choice, item)) return false;\n'
        '*chosen_item = choice.item; *action = choice.action; *shield_item = choice.shield_item; return true; }\n'
        'bool test_apply_active_item(int item) { active_weapon_choice choice;\n'
        'if (!choose_active_weapon(&choice, item)) return false;\n'
        'apply_active_weapon_choice_internal(&choice); return true; }\n', encoding="utf-8")
    excluded = ("/src/main.c.obj", "/src/cmd/ui/cmd-ui-knowledge.c.obj",
                "/src/player/player-active-weapon.c.obj")
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
               "ui_question_ask_objects_with_help", "do_cmd_active_item",
               "calc_bonuses_for_preview", "update_stuff")
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17",
        "-O0", "-g", "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), str(wrapper),
        "@" + str(rsp), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
        *["-Wl,--wrap=" + s for s in wrapped], "-o", str(exe)],
        cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe)], cwd=ROOT, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Check dynamic weapon categories, Back navigation and original choice mapping.

Links the production picker against the configured standard build. Synthetic
gear exercises menu navigation without opening a player profile or spending turns.
"""
from pathlib import Path
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/active-weapon-groups"
HARNESS = r'''
#include "angband.h"
#include "player/player-active-weapon.c"
#include <assert.h>

static player_type player;
static object_type gear[INVEN_TOTAL];
static object_kind kinds[4];
static active_weapon_choice choices[13];
static ui_question_option options[13];
static const object_type* icons[13];
static const char* titles[16];
static const char* picks[16];
static int counts[16], defaults[16], step, steps;
static bool throwing_disabled, no_ranged;

int __wrap_ui_question_ask_objects_with_help(cptr title, cptr help,
    const ui_question_option* rows, const object_type* const* row_icons,
    int count, int y, int x, int def)
{
    assert(step < steps && streq(title, titles[step]));
    assert(streq(help, "Setup help") && y == UI_QUESTION_GLOBAL
        && x == UI_QUESTION_GLOBAL && count == counts[step]);
    assert(def >= 0 && def < count);
    if (defaults[step] >= 0) assert(def == defaults[step]);
    for (int i = 0; i < count; i++) {
        assert(rows[i].key == active_weapon_menu_key(i));
        for (int j = 0; j < i; j++)
            assert(!rows[i].key || rows[i].key != rows[j].key);
        if (strstr(rows[i].label, "Throwing ("))
            assert(rows[i].disabled == throwing_disabled);
        if (no_ranged) assert(!strstr(rows[i].label, "Ranged ("));
        if (streq(rows[i].label, "Harness [stored]"))
            assert(rows[i].disabled);
    }
    if (streq(rows[0].label, "Back to categories")) {
        assert(!row_icons[0] && !rows[0].disabled);
        for (int i = 1; i < count; i++) {
            bool found = false;
            for (int j = 0; j < 10; j++)
                if (rows[i].label == options[j].label) {
                    assert(row_icons[i] == icons[j]);
                    assert(rows[i].attr == options[j].attr);
                    assert(rows[i].disabled == options[j].disabled);
                    found = true;
                }
            assert(found);
        }
    }
    const char* pick = picks[step++];
    if (!pick) return -1;
    for (int i = 0; i < count; i++)
        if (streq(rows[i].label, pick)) {
            assert(!rows[i].disabled);
            return i;
        }
    assert(!"Missing menu choice"); return -1;
}
static void expect(cptr title, cptr pick, int count, int def)
{
    titles[steps] = title; picks[steps] = pick;
    counts[steps] = count; defaults[steps++] = def;
}
static void run(int count, int def, int result)
{
    player_type before = player;
    object_type before_gear[INVEN_TOTAL];
    memcpy(before_gear, gear, sizeof(gear));
    assert(ask_active_weapon_menu("Choose item setup", "Setup help",
        choices, options, icons, count, def) == result);
    assert(step == steps);
    assert(!memcmp(&before, &player, sizeof(player)));
    assert(!memcmp(before_gear, gear, sizeof(gear)));
    step = steps = 0;
}
int main(void)
{
    /* The ordinary desktop frontend must get the same categories as mobile. */
    assert(!sdl_handheld_ui_active());
    p_ptr = &player; inventory = gear; k_info = kinds;
    kinds[1].tval = TV_SWORD;
    kinds[2].tval = TV_HAFTED; kinds[2].flags3 = TR3_TWO_HANDED;
    kinds[3].tval = TV_SWORD; kinds[3].flags3 = TR3_HAND_AND_A_HALF;
    gear[0] = (object_type){ .k_idx = 1, .tval = TV_SWORD, .number = 1 };
    gear[1] = (object_type){ .k_idx = 2, .tval = TV_HAFTED, .number = 1 };
    gear[2] = (object_type){ .k_idx = 3, .tval = TV_SWORD, .number = 1 };
    gear[INVEN_ARM] = (object_type){ .k_idx = 1, .tval = TV_SHIELD, .number = 1 };
    player.energy_use = 37;
    for (int i = 0; i < 13; i++) {
        choices[i].action = ACTIVE_ITEM_SETUP;
        choices[i].o_ptr = &gear[0]; choices[i].item = i;
        choices[i].mode = PLAYER_ACTIVE_WEAPON_MELEE;
        choices[i].kind = PLAYER_ACTIVE_WEAPON_KIND_MELEE;
        choices[i].target_slot = INVEN_WIELD;
        choices[i].shield_item = ACTIVE_WEAPON_SHIELD_NONE;
        strnfmt(choices[i].label, sizeof(choices[i].label), "Choice %d", i);
        options[i] = (ui_question_option){ active_weapon_menu_key(i),
            choices[i].label, (byte)(TERM_WHITE + i % 3), false };
        icons[i] = choices[i].o_ptr;
    }
    choices[1].shield_item = -1; /* Retained equipped shield. */
    choices[2].shield_item = INVEN_ARM;
    choices[2].shield_object = gear[INVEN_ARM];
    SDL_strlcpy(choices[2].label, "Choice 2 [active]", sizeof(choices[2].label));
    choices[3].o_ptr = &gear[1]; icons[3] = &gear[1];
    choices[4].o_ptr = &gear[2]; icons[4] = &gear[2];
    for (int i = 5; i <= 6; i++) {
        choices[i].shield_item = 0; choices[i].shield_object = gear[0];
    }
    choices[7].kind = PLAYER_ACTIVE_WEAPON_KIND_BOW;
    choices[8].kind = choices[9].kind = PLAYER_ACTIVE_WEAPON_KIND_THROWING;
    choices[10].action = ACTIVE_ITEM_HARNESS;
    choices[11].action = ACTIVE_ITEM_PACK; choices[12].action = ACTIVE_ITEM_BELT;
    SDL_strlcpy(choices[10].label, "Harness [stored]", sizeof(choices[10].label));
    SDL_strlcpy(choices[11].label, "Store in Pack", sizeof(choices[11].label));
    SDL_strlcpy(choices[12].label, "Equip on Belt", sizeof(choices[12].label));
    options[10].disabled = true;
    const int groups[] = { 0,1,1,2,2,3,3,4,5,5,-1,-1,-1 };
    for (int i = 0; i < 13; i++) assert(active_weapon_choice_group(&choices[i]) == groups[i]);
    /* A hand-and-a-half weapon is two-handed only without a shield. */
    active_weapon_choice half = choices[4];
    half.shield_item = -1;
    assert(active_weapon_choice_group(&half) == ACTIVE_WEAPON_GROUP_SHIELD);
    puts("PASS: shield retention/removal, two-handed, hand-and-a-half, dual, ranged and throwing classification.");

    /* Insertion of a later melee row must not steal an active throwing row's
     * default while sorting. Its category should open on the current setup. */
    active_weapon_choice sorted[] = { choices[0], choices[8], choices[4] };
    ui_question_option sorted_options[] = { options[0], options[8], options[4] };
    int sorted_default = 1;
    prepare_active_weapon_menu_choices(sorted, sorted_options, 3, &sorted_default);
    assert(sorted_default == 0 && sorted[0].kind == PLAYER_ACTIVE_WEAPON_KIND_THROWING);
    puts("PASS: sorting keeps the active throwing setup as the selection default.");

    const char* names[] = { "Melee (1)", "Melee + shield (2) [active]",
        "Two-handed melee (2)", "Two weapons (2)", "Ranged (1)", "Throwing (2)" };
    const char* sub[] = { "Melee", "Melee + shield", "Two-handed melee", "Two weapons", "Ranged", "Throwing" };
    const int source[] = { 0,2,4,6,7,9 }, sizes[] = { 2,3,3,3,2,3 };
    for (int group = 0; group < 6; group++) {
        expect("Choose item setup", names[group], 9, 1);
        expect(sub[group], choices[source[group]].label, sizes[group], group == 1 ? 2 : 1);
        run(13, 2, source[group]);
    }
    puts("PASS: all six groups select the original setup, icon, colour and active default.");
    expect("Choose item setup", names[3], 9, 1);
    expect(sub[3], "Back to categories", 3, 1);
    expect("Choose item setup", names[4], 9, 3);
    expect(sub[4], NULL, 2, 1);
    expect("Choose item setup", NULL, 9, 4);
    run(13, 2, -1);
    expect("Choose item setup", "Store in Pack", 9, 1);
    run(13, 2, 11);
    throwing_disabled = options[8].disabled = options[9].disabled = true;
    expect("Choose item setup", NULL, 9, 1); run(13, 2, -1);
    throwing_disabled = options[8].disabled = options[9].disabled = false;
    puts("PASS: Back, Escape/close, root cancellation, direct storage and disabled tutorial groups preserve game state.");

    expect("Choose item setup", choices[1].label, 2, 1); run(2, 1, 1);
    expect("Choose item setup", choices[7].label, 8, 2); run(8, 2, 7);
    expect("Choose item setup", "Throwing (1)", 6, 1);
    expect(sub[5], choices[8].label, 2, 1); run(9, 2, 8);
    expect("Choose item setup", names[5], 9, 1);
    expect(sub[5], choices[9].label, 3, 1); run(13, 2, 9);
    choices[7].kind = PLAYER_ACTIVE_WEAPON_KIND_THROWING; no_ranged = true;
    expect("Choose item setup", NULL, 8, 1); run(13, 2, -1); no_ranged = false;
    for (int i = 0; i < 10; i++) choices[i] = choices[0];
    expect("Choose item setup", choices[0].label, 13, 0); run(13, 0, 0);
    puts("PASS: two-setup and other short mixed lists stay direct; larger desktop lists use relevant categories; single-category lists stay direct.");
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/player/player-active-weapon.c.obj")
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects if not p.endswith(excluded)))
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / dep) for dep in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp",
        "-Wl,--wrap=ui_question_ask_objects_with_help", "-o", str(exe)],
        cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe)], cwd=OUT, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()

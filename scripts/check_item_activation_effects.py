#!/usr/bin/env python3
"""Exercise consumable curse routing, force-horn line of sight and submerged gems."""
import os
import shlex
import subprocess
import tempfile

from check_new_monsters import BUILD, HARNESS, ROOT

OUT = ROOT / "scripts/output/item-activation-effects"
CHECKS = r'''
#include "metarun.h"

static int aim_dir = 6;
static int light_calls;
static int knockback_calls;
bool __real_light_area(int dd, int ds, int radius);
bool __real_knock_back_observing_fall(int y1, int x1, int y2, int x2, bool* saw_fall);

bool __wrap_get_aim_dir(int* dir, int range)
{
    assert(range == 0);
    *dir = aim_dir;
    return true;
}

bool __wrap_light_area(int dd, int ds, int radius)
{
    assert(dd == 4 && ds == 4 && radius == 7);
    light_calls++;
    return __real_light_area(dd, ds, radius);
}

bool __wrap_knock_back_observing_fall(int y1, int x1, int y2, int x2, bool* saw_fall)
{
    knockback_calls++;
    return __real_knock_back_observing_fall(y1, x1, y2, x2, saw_fall);
}

bool __wrap_get_check(cptr prompt)
{
    assert(strstr(prompt, "Autoinscribe"));
    return false;
}

void __wrap_handle_stuff(void) {}
void __wrap_message_flush(void) {}
void __wrap_monster_perception(bool centered, bool main_roll, int difficulty)
{
    (void)centered; (void)main_roll; (void)difficulty;
}

static int prepare(object_type* item, int tval, int sval, int count)
{
    int kind = lookup_kind(tval, sval);
    assert(kind);
    object_prep(item, kind);
    item->number = count;
    k_info[kind].aware = k_info[kind].tried = false;
    return kind;
}

static void reset_action(void)
{
    reset_map(10);
    memset(inventory, 0, INVEN_TOTAL * sizeof(*inventory));
    player_carried_extra_reset_store();
    player_quiver_reset_store();
    player_pack_action_reset();
    supplies_reset_store();
    p_ptr->food = 4000;
    p_ptr->image = p_ptr->energy_use = 0;
    p_ptr->previous_action[0] = ACTION_NOTHING;
    p_ptr->mhp = p_ptr->chp = 50;
    p_ptr->msp = p_ptr->csp = 40;
}

static int roll_seed(int minimum, int maximum)
{
    for (u64b seed = 1; seed < 1000; seed++)
    {
        Rand_state_init(seed);
        int roll = rand_int(100);
        if (roll >= minimum && roll < maximum)
        {
            int duration = 11 + rand_int(10);
            Rand_state_init(seed);
            return duration;
        }
    }
    assert(false);
    return 0;
}

static bool saw_message(const char* text)
{
    for (int i = 0; i < message_num(); i++)
    {
        if (strstr(message_str(i), text))
            return true;
    }
    return false;
}

static void check_delirium(void)
{
    reset_action();
    character_generated = true;
    const int curse = 30;
    assert(cu_info[curse].flags_u & CUR_HALLU);
    CURSE_SET(curse, 2);
    assert(curse_flag_count_cur(CUR_HALLU) == 2);

    /* Roll between the one-stack and two-stack thresholds. The potion's
     * ordinary effect has no visible result because Voice is already full. */
    object_type potion;
    int kind = prepare(&potion, TV_POTION, SV_POTION_VOICE, 2);
    int floor = floor_carry(p_ptr->py, p_ptr->px, &potion);
    assert(floor > 0);
    int duration = roll_seed(20, 40);
    do_cmd_quaff_potion(&o_list[floor], -floor);
    assert(p_ptr->image == duration && p_ptr->energy_use == 100);
    assert(saw_message("You suddenly see impossible colors!"));
    assert(o_list[floor].number == 1 && !k_info[kind].aware && k_info[kind].tried);

    p_ptr->image = 0;
    CURSE_SET(curse, 1);
    roll_seed(20, 40);
    do_cmd_quaff_potion(&o_list[floor], -floor);
    assert(!p_ptr->image && !o_list[floor].k_idx);

    prepare(&inventory[0], TV_STAFF, SV_STAFF_IMPRISONMENT, 1);
    inventory[0].pval = 2 * CHANNELING_CHARGE_MULTIPLIER;
    roll_seed(0, 20);
    do_cmd_activate_staff(&inventory[0], 0);
    assert(!p_ptr->image && inventory[0].pval == CHANNELING_CHARGE_MULTIPLIER);

    object_type gem;
    prepare(&gem, TV_GEM, SV_GEM_TREASURES, 2);
    assert(supplies_absorb_object(&gem));
    roll_seed(0, 20);
    supplies_begin_action(0);
    do_cmd_use_gem(supplies_entry_at(0), SUPPLIES_INDEX);
    supplies_end_action();
    assert(!p_ptr->image && supplies_entry_units(0) == 1);
    puts("Draught of Delirium triggers only on quaff, with stacking odds and no false identification PASS.");
}

static void check_delirium_immunity(void)
{
    reset_action();
    character_generated = true;
    p_ptr->have_ability[S_SPC][SPC_MANDOS] = true;
    CURSE_SET(30, 1);
    assert(curse_flag_count_cur(CUR_HALLU) == 1);

    object_type potion;
    int kind = prepare(&potion, TV_POTION, SV_POTION_VOICE, 1);
    int floor = floor_carry(p_ptr->py, p_ptr->px, &potion);
    assert(floor > 0);
    roll_seed(0, 20);
    do_cmd_quaff_potion(&o_list[floor], -floor);
    assert(!p_ptr->image && p_ptr->energy_use == 100);
    assert(!saw_message("You suddenly see impossible colors!"));
    assert(!o_list[floor].k_idx && !k_info[kind].aware && k_info[kind].tried);
    puts("Mandos' Doom prevents Delirium hallucination and its message without changing potion use PASS.");
}

static void check_force(void)
{
    reset_action();
    prepare(&inventory[0], TV_HORN, SV_HORN_FORCE, 1);
    p_ptr->py = p_ptr->px = 10;
    p_ptr->skill_use[S_WIL] = 60;
    Rand_state_init(13);
    cave_set_feat(10, 11, FEAT_WALL_EXTRA);
    assert(!los(10, 10, 10, 12) && los(10, 10, 9, 11));
    assert(place_monster_one(10, 12, 11, false, false, NULL));
    assert(place_monster_one(9, 11, 11, false, false, NULL));
    monster_type* blocked = &mon_list[cave_m_idx[10][12]];
    monster_type* reached = &mon_list[cave_m_idx[9][11]];
    do_cmd_play_instrument(&inventory[0], 0);
    assert(blocked->fy == 10 && blocked->fx == 12 && !blocked->stunned);
    assert(reached->fy < 9 && reached->fx > 11 && reached->stunned);
    assert(p_ptr->energy_use == 100 && p_ptr->csp == 20);

    reset_action();
    prepare(&inventory[0], TV_HORN, SV_HORN_FORCE, 1);
    p_ptr->py = p_ptr->px = 1;
    aim_dir = 8;
    do_cmd_play_instrument(&inventory[0], 0);
    assert(p_ptr->energy_use == 100 && p_ptr->csp == 20);
    puts("Horn of Force pays normally, pushes reachable enemies, and stops at walls and map edges PASS.");
}

static void check_force_chasm(bool visible_before, bool visible_on_landing)
{
    reset_action();
    character_generated = true;
    int kind = prepare(&inventory[0], TV_HORN, SV_HORN_FORCE, 1);
    p_ptr->py = p_ptr->px = 10;
    p_ptr->skill_use[S_WIL] = 60;
    Rand_state_init(13);
    cave_set_feat(10, 12, FEAT_CHASM);
    if (visible_before)
        cave_info[10][11] |= CAVE_VIEW | CAVE_SEEN;
    if (visible_on_landing)
        cave_info[10][12] |= CAVE_VIEW | CAVE_SEEN;
    assert(place_monster_one(10, 11, 11, false, false, NULL));
    int target = cave_m_idx[10][11];
    assert(mon_list[target].ml == visible_before && !(r_info[11].flags2 & RF2_FLYING));
    mon_list[target].hp = 100;
    knockback_calls = 0;

    do_cmd_play_instrument(&inventory[0], 0);
    assert(!cave_m_idx[10][11] && !cave_m_idx[10][12]);
    assert(saw_message("topples into the abyss!") == visible_on_landing);
    assert(knockback_calls == 1);
    assert(!mon_list[target].r_idx && !mon_list[target].stunned && !mon_list[target].alertness);
    assert(k_info[kind].aware == (visible_before || visible_on_landing));
    assert(k_info[kind].tried);
    assert(p_ptr->energy_use == 100 && p_ptr->csp == 20);
    printf("First-push chasm fall with visibility %d before push and %d on landing PASS.\n",
        visible_before, visible_on_landing);
}

static void check_submerged_gems(void)
{
    reset_action();
    p_ptr->py = p_ptr->px = 10;
    p_ptr->skill_use[S_WIL] = 60;
    Rand_state_init(43);
    int y = p_ptr->py, x = p_ptr->px;
    cave_set_feat(y, x, FEAT_DEEP_WATER);
    assert(player_submerged_in_deep_water());
    assert((r_info[21].flags3 & RF3_HURT_LITE)
        && place_monster_one(y, x+1, 21, false, false, NULL));
    monster_type* target = &mon_list[cave_m_idx[y][x+1]];
    int hp = target->hp;
    cave_info[y][x+1] |= CAVE_VIEW;
    cave_light[y][x+1] = 5;
    object_type gem;
    int kind = prepare(&gem, TV_GEM, SV_GEM_LIGHT, 2);
    assert(supplies_absorb_object(&gem));
    supplies_begin_action(0);
    do_cmd_use_gem(supplies_entry_at(0), SUPPLIES_INDEX);
    supplies_end_action();
    assert(!light_calls && !p_ptr->energy_use && !k_info[kind].tried);
    assert(supplies_entry_units(0) == 2);

    object_copy(&gem, supplies_entry_at(0));
    int floor = floor_carry(y, x, &gem);
    assert(floor > 0);
    do_cmd_use_gem(&o_list[floor], -floor);
    assert(!light_calls && !p_ptr->energy_use && o_list[floor].number == 2);
    assert(!target->stunned && target->hp == hp);

    prepare(&gem, TV_GEM, SV_GEM_TREASURES, 2);
    assert(supplies_absorb_object(&gem));
    supplies_begin_action(1);
    do_cmd_use_gem(supplies_entry_at(1), SUPPLIES_INDEX);
    supplies_end_action();
    assert(!light_calls && p_ptr->energy_use == 100 && supplies_entry_units(1) == 1);

    cave_set_feat(y, x, FEAT_BRIDGE_DEEP_WATER_H);
    assert(!player_submerged_in_deep_water());
    do_cmd_use_gem(&o_list[floor], -floor);
    assert(light_calls == 1 && p_ptr->energy_use == 100 && o_list[floor].number == 1);
    assert(target->stunned && target->hp < hp);
    puts("Submersion stops the same light attack from carried and floor gems; detection and dry bridge use work PASS.");
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix, init = HARNESS.split("int main(int argc,char** argv)", 1)
    init = init.split("    check_templates();", 1)[0].replace("argc==3", "argc==4")
    source = OUT / "check.c"
    source.write_text(prefix + CHECKS + "int main(int argc,char** argv)" + init +
                      "    assert(init_flavor_info()==0); flavor_init();\n"
                      '    if (streq(argv[3], "curse")) check_delirium();\n'
                      '    if (streq(argv[3], "curse-immunity")) check_delirium_immunity();\n'
                      '    if (streq(argv[3], "horn")) check_force();\n'
                      '    if (streq(argv[3], "horn-chasm")) check_force_chasm(true, true);\n'
                      '    if (streq(argv[3], "horn-landing")) check_force_chasm(false, true);\n'
                      '    if (streq(argv[3], "horn-dark-landing")) check_force_chasm(true, false);\n'
                      '    if (streq(argv[3], "horn-unseen")) check_force_chasm(false, false);\n'
                      '    if (streq(argv[3], "gems")) check_submerged_gems();\n'
                      "    SDL_Quit(); return 0;\n}\n", encoding="utf-8")
    sources = ("src/use-obj.c", "src/cmd/item/cmd-item-activate.c", "src/player/effects.c",
               "src/cmd/combat/cmd-combat.c", "src/monster/monster-move.c")
    excluded = ("/src/main.c.obj", *(f"/{path}.obj" for path in sources))
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join(f'"{obj}"' for obj in objects if not obj.endswith(excluded)),
                        encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / dep) for dep in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"],
    ])
    wraps = ("get_aim_dir", "knock_back_observing_fall", "light_area", "get_check",
             "handle_stuff", "message_flush", "monster_perception")
    exe = OUT / "check.exe"
    subprocess.run([
        "C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
        *(str(ROOT / path) for path in sources), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", *(f"-Wl,--wrap={name}" for name in wraps),
        "-o", str(exe),
    ], cwd=BUILD, env=env, check=True)
    failed = []
    for case in ("curse", "curse-immunity", "horn", "horn-chasm", "horn-landing",
                 "horn-dark-landing", "horn-unseen", "gems"):
        with tempfile.TemporaryDirectory(prefix="fixture-", dir=OUT) as state:
            result = subprocess.run([str(exe), str(ROOT / "lib/edit"), state, case],
                                    cwd=state, env=env, timeout=15)
            if result.returncode:
                failed.append(case)
    if failed:
        raise SystemExit(f"Failed cases: {', '.join(failed)}")


if __name__ == "__main__":
    main()

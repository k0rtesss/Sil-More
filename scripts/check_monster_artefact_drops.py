#!/usr/bin/env python3
"""Exercise artefact ownership on successful and blocked monster drops."""

import os
import shlex
import subprocess
import tempfile

from qa9_sdl_fixture import BUILD, HARNESS, ROOT


OUT = ROOT / "scripts/output/monster-artefact-drops"
SOURCES = (
    "src/world/monster-death.c",
    "src/cmd/monster/cmd-monster-alert.c",
    "src/cmd/combat/cmd-combat.c",
    "src/cmd/combat/cmd-ranged.c",
)

CHECKS = r'''
static bool saw_crown_fall;
void __wrap_msg_print(cptr message)
{
    if (message && (!strcmp(message, "The Iron Crown falls.")
        || strstr(message, "falls to the ground nearby.")))
        saw_crown_fall = true;
}
void __wrap_message_flush(void) {}
bool __wrap_get_check(cptr prompt)
{
    assert(strstr(prompt, "prise a Silmaril"));
    return true;
}
bool __wrap_get_aim_dir(int* dir, int range)
{
    assert(range > 0);
    *dir = 6; /* Morgoth is due east of the player in these fixtures. */
    return true;
}

static void prepare_level(bool blocked)
{
    wipe_o_list();
    wipe_mon_list();
    player_wipe();
    rp_ptr = &p_info[0];
    current_character_profile = &c_info[0];
    p_ptr->playing = true;
    p_ptr->tutorial_deferred = true;
    p_ptr->depth = p_ptr->max_depth = 19;
    p_ptr->cur_map_hgt = p_ptr->cur_map_wid = 30;
    p_ptr->py = 10;
    p_ptr->px = 9;
    p_ptr->chp = p_ptr->mhp = 100;
    character_generated = character_dungeon = true;
    for (int y = 0; y < 30; ++y) {
        for (int x = 0; x < 30; ++x) {
            cave_o_idx[y][x] = cave_m_idx[y][x] = 0;
            cave_info[y][x] = 0;
            bool edge = !y || !x || y == 29 || x == 29;
            cave_set_feat(y, x, edge || blocked ? FEAT_WALL_PERM : FEAT_FLOOR);
        }
    }
    if (blocked) {
        /* Walkable stairs with no open floor, so drop_near cannot land. */
        cave_set_feat(10, 9, FEAT_LESS);
        cave_set_feat(10, 10, FEAT_MORE);
    }
    cave_m_idx[10][9] = -1;
    turn = 1900;
    playerturn = 190;
    Rand_state_init(37);
}

static int place_target(int race)
{
    monster_type target = {.r_idx = race, .hp = 100, .maxhp = 100,
        .alertness = ALERTNESS_VERY_ALERT, .ml = true};
    int index = monster_place(10, 10, &target);
    assert(index > 0);
    return index;
}

static int floor_artefact(int artefact)
{
    int result = 0;
    for (int i = 1; i < o_max; ++i) {
        const object_type* item = &o_list[i];
        if (item->k_idx && item->name1 == artefact) {
            assert(!result && !item->held_m_idx && item->number == 1);
            assert(cave_o_idx[item->iy][item->ix] == i && !item->next_o_idx);
            result = i;
        }
    }
    return result;
}

static void check_gothmog(bool blocked)
{
    prepare_level(blocked);
    for (int i = 1; i < z_info->art_max; ++i)
        a_info[i].cur_num = 0;
    assert(r_info[R_IDX_GOTHMOG].flags1 & RF1_DROP_CHOSEN);
    assert(r_info[R_IDX_GOTHMOG].flags3 & RF3_DROP_ARTEFACT);
    int index = place_target(R_IDX_GOTHMOG);
    assert(mon_take_hit(index, 100, "", -1));
    assert(!mon_list[index].r_idx && !r_info[R_IDX_GOTHMOG].max_num);
    assert((floor_artefact(ART_CALRIS) != 0) == !blocked);
    int other_artefacts = 0;
    for (int i = 1; i < z_info->art_max; ++i) {
        if (i != ART_CALRIS && a_info[i].cur_num) {
            assert(floor_artefact(i));
            ++other_artefacts;
        }
    }
    assert((other_artefacts > 0) == !blocked);
    assert((a_info[ART_CALRIS].cur_num != 0) == !blocked);
    if (blocked)
        assert(!o_cnt && !p_ptr->artefacts);
}

static void check_carried(bool blocked, bool seen, bool chasm)
{
    prepare_level(blocked);
    int index = place_target(R_IDX_MAEGLIN);
    object_type sword, potion;
    artefact_type* artefact = &a_info[ART_ORCRIST];
    object_prep(&sword, lookup_kind(artefact->tval, artefact->sval));
    sword.name1 = ART_ORCRIST;
    apply_magic(&sword, -1, true, true, true, true);
    artefact->seen = seen ? ART_SEEN_PHYSICAL : 0;
    artefact->found_num = 0;
    sword.ident |= IDENT_HIDE_CARRY;
    object_prep(&potion, lookup_kind(TV_POTION, SV_POTION_HEALING));
    potion.number = 2;
    potion.obj_note = quark_add("carried fixture");
    int sword_idx = monster_carry(index, &sword);
    int potion_idx = monster_carry(index, &potion);
    assert(sword_idx && potion_idx && mon_list[index].hold_o_idx == potion_idx);
    assert(o_list[potion_idx].next_o_idx == sword_idx);
    if (chasm) {
        assert(!(r_info[R_IDX_MAEGLIN].flags2 & RF2_FLYING));
        cave_set_feat(10, 10, FEAT_CHASM);
    }
    assert(mon_take_hit(index, 100, "", -1));
    assert(!mon_list[index].r_idx && !mon_list[index].hold_o_idx);
    int item = floor_artefact(ART_ORCRIST);
    assert((item != 0) == !blocked);
    assert((artefact->cur_num != 0) == (!blocked || seen));
    if (item)
        assert(!(o_list[item].ident & IDENT_HIDE_CARRY));
    int potions = 0;
    for (int i = 1; i < o_max; ++i)
        if (o_list[i].k_idx == potion.k_idx
            && o_list[i].obj_note == potion.obj_note) {
            assert(!o_list[i].held_m_idx && !o_list[i].next_o_idx);
            potions += o_list[i].number;
        }
    assert(potions == (blocked ? 0 : 2));
}

static void check_crown(void)
{
    prepare_level(true);
    int index = place_target(R_IDX_MORGOTH);
    p_ptr->morgoth_hits = 1;
    birth_no_artefacts = true; /* The Crown is a required exception. */
    int kind = lookup_kind(a_info[ART_MORGOTH_3].tval, a_info[ART_MORGOTH_3].sval);
    k_info[kind].aware = false;
    s32b old_xp = p_ptr->new_exp;
    saw_crown_fall = false;
    drop_iron_crown(&mon_list[index], "The Iron Crown falls.");
    assert(!a_info[ART_MORGOTH_3].cur_num && !floor_artefact(ART_MORGOTH_3));
    assert(!saw_crown_fall && !p_ptr->morgoth_state && p_ptr->morgoth_hits == 1);
    assert(!k_info[kind].aware && p_ptr->new_exp == old_xp);

    cave_set_feat(9, 10, FEAT_FLOOR);
    drop_iron_crown(&mon_list[index], "The Iron Crown falls.");
    int crown = floor_artefact(ART_MORGOTH_3);
    assert(crown && o_list[crown].tval == TV_CROWN && object_known_p(&o_list[crown]));
    assert(saw_crown_fall && a_info[ART_MORGOTH_3].cur_num && p_ptr->morgoth_state == 1);
    saw_crown_fall = false;
    drop_iron_crown(&mon_list[index], "The Iron Crown falls.");
    assert(!saw_crown_fall && floor_artefact(ART_MORGOTH_3) == crown);
    /* A Crown that really landed and was subsequently lost stays unique. */
    delete_object_idx(crown);
    assert(!drop_iron_crown(&mon_list[index], "The Iron Crown falls."));
    assert(a_info[ART_MORGOTH_3].cur_num && !floor_artefact(ART_MORGOTH_3));
}

static void check_melee_crown(void)
{
    prepare_level(true);
    p_ptr->depth = MORGOTH_DEPTH;
    p_ptr->active_weapon_mode = PLAYER_ACTIVE_WEAPON_MELEE;
    p_ptr->skill_use[S_MEL] = 100;
    p_ptr->mdd = 24;
    p_ptr->mds = 2;
    op_ptr->opt[OPT_pacifist_attack_warning] = false;
    object_prep(&inventory[INVEN_WIELD], lookup_kind(TV_SWORD, SV_LONG_SWORD));
    p_ptr->equip_cnt = 1;
    r_info[R_IDX_MORGOTH].pd = 0;
    int index = place_target(R_IDX_MORGOTH);
    mon_list[index].hp = mon_list[index].maxhp = 10000;

    py_attack_aux(10, 10, ATT_MAIN);
    assert(p_ptr->morgoth_hits == 1 && !a_info[ART_MORGOTH_3].cur_num);
    int hp = mon_list[index].hp;
    saw_crown_fall = false;
    py_attack_aux(10, 10, ATT_MAIN);
    assert(mon_list[index].hp <= hp - 10);
    assert(p_ptr->morgoth_hits == 1 && !p_ptr->morgoth_state && !saw_crown_fall);
    assert(!a_info[ART_MORGOTH_3].cur_num && !floor_artefact(ART_MORGOTH_3));

    cave_set_feat(9, 10, FEAT_FLOOR);
    py_attack_aux(10, 10, ATT_MAIN);
    int crown = floor_artefact(ART_MORGOTH_3);
    assert(crown && p_ptr->morgoth_hits == 2 && p_ptr->morgoth_state == 1);
    assert(saw_crown_fall);

    int y = o_list[crown].iy, x = o_list[crown].ix;
    cave_m_idx[p_ptr->py][p_ptr->px] = 0;
    assert(player_place(y, x));
    p_ptr->stat_base[A_STR] = 20;
    p_ptr->mdd = 100;
    p_ptr->mds = 1;
    assert(do_cmd_delete_item_by_index(-crown));
    assert(silmarils_possessed() == 1 && o_list[crown].name1 == ART_MORGOTH_2);
}

static void make_projectile_attack(bool thrown)
{
    p_ptr->energy_use = 0;
    if (thrown)
        do_cmd_throw_from_slot(INVEN_WIELD);
    else
        do_cmd_fire(1);
    assert(p_ptr->energy_use == 100);
}

static void check_projectile_crown(bool thrown)
{
    prepare_level(true);
    p_ptr->depth = MORGOTH_DEPTH;
    p_ptr->morgoth_hits = 1;
    p_ptr->active_weapon_mode = PLAYER_ACTIVE_WEAPON_RANGED_1;
    p_ptr->skill_use[S_MEL] = p_ptr->skill_use[S_ARC] = 100;
    p_ptr->skill_base[S_MEL] = p_ptr->skill_base[S_ARC] = 100;
    p_ptr->stat_base[A_STR] = p_ptr->stat_use[A_STR] = 8;
    p_ptr->equip_cnt = 1;
    op_ptr->opt[OPT_pacifist_attack_warning] = false;
    if (thrown) {
        object_type* spear = &inventory[INVEN_WIELD];
        object_prep(spear, lookup_kind(TV_POLEARM, SV_SPEAR));
        spear->number = 3;
        spear->dd = 24;
        spear->ds = 2;
        spear->pickup_slot = PICKUP_SLOT_ACTIVE_THROWING;
    } else {
        object_type arrow;
        object_prep(&inventory[INVEN_BOW], lookup_kind(TV_BOW, SV_SHORT_BOW));
        object_prep(&arrow, lookup_kind(TV_ARROW, SV_NORMAL_ARROW));
        arrow.number = 3;
        assert(player_quiver_absorb_arrow(&arrow) == 3);
        p_ptr->ammo_tval = TV_ARROW;
        p_ptr->add = 24;
        p_ptr->ads = 4;
    }
    r_info[R_IDX_MORGOTH].pd = 0;
    int index = place_target(R_IDX_MORGOTH);
    mon_list[index].hp = mon_list[index].maxhp = 10000;
    cave_info[10][9] |= CAVE_VIEW | CAVE_SEEN | CAVE_FIRE;
    cave_info[10][10] |= CAVE_VIEW | CAVE_SEEN | CAVE_FIRE;
    player_active_weapon_begin_player_turn();

    int hp = mon_list[index].hp;
    saw_crown_fall = false;
    make_projectile_attack(thrown);
    assert(mon_list[index].hp <= hp - 10);
    assert(p_ptr->morgoth_hits == 1 && !p_ptr->morgoth_state && !saw_crown_fall);
    assert(!a_info[ART_MORGOTH_3].cur_num && !floor_artefact(ART_MORGOTH_3));

    hp = mon_list[index].hp;
    cave_set_feat(9, 10, FEAT_FLOOR);
    player_active_weapon_begin_player_turn();
    make_projectile_attack(thrown);
    assert(mon_list[index].hp <= hp - 10);
    assert(p_ptr->morgoth_hits == 2 && p_ptr->morgoth_state == 1 && saw_crown_fall);
    int crown = floor_artefact(ART_MORGOTH_3);
    assert(a_info[ART_MORGOTH_3].cur_num && crown && o_list[crown].tval == TV_CROWN);
    puts(thrown ? "A thrown spear retries Crown placement after a blocked heavy hit."
        : "A fired arrow retries Crown placement after a blocked heavy hit.");
}

static void checks(cptr scenario)
{
    prepare_level(false);
    flavor_init();
    drop_system_init();
    if (!strcmp(scenario, "fresh")) {
        check_gothmog(false);
        check_gothmog(true);
        puts("Fixed and catalog artefacts have exactly one owner after placement and no phantom reservation when blocked.");
    } else if (!strcmp(scenario, "carried")) {
        check_carried(false, false, false);
        check_carried(true, false, false);
        check_carried(true, true, false);
        check_carried(true, false, true);
        puts("Carried items leave the monster chain; a lost unseen relic remains eligible and a seen relic remains unique.");
    } else {
        assert(!strcmp(scenario, "crown"));
        check_crown();
        check_melee_crown();
        puts("The blocked Crown stays on Morgoth's brow. The next heavy hit releases the intact Crown and a Silmaril can be prised.");
        check_projectile_crown(false);
        check_projectile_crown(true);
    }
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / dep) for dep in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"],
    ])
    env.update(SDL_VIDEO_DRIVER="dummy", SDL_RENDER_DRIVER="software",
        SDL_AUDIO_DRIVER="dummy", CONTROL_TEST_ASSETS=str(ROOT / "lib"))
    prefix = HARNESS[:HARNESS.index("static cptr make_path")]
    init = HARNESS[HARNESS.index("static cptr make_path"):HARNESS.index("    int last = ")]
    source = OUT / "check.c"
    source.write_text(prefix + CHECKS + init
        + "    checks(SDL_getenv(\"LOOT_FIXTURE\")); sdl_quit_hook(NULL); return 0;\n}\n",
        encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", *("/" + path + ".obj" for path in SOURCES))
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects
        if not obj.endswith(excluded)), encoding="utf-8")
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
        *(str(ROOT / path) for path in SOURCES),
        "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
        "-Wl,--wrap=msg_print", "-Wl,--wrap=message_flush", "-Wl,--wrap=get_check",
        "-Wl,--wrap=get_aim_dir",
        "-o", str(exe)], cwd=BUILD, env=env, check=True)
    results = []
    for scenario in ("fresh", "carried", "crown"):
        with tempfile.TemporaryDirectory(prefix="profile-", dir=OUT) as profile:
            result = subprocess.run([str(exe), "--windowed", "--tiles"], cwd=ROOT,
                env=dict(env, CONTROL_TEST_PROFILE=profile, LOOT_FIXTURE=scenario),
                timeout=60)
            results.append(result)
    for result in results:
        result.check_returncode()


if __name__ == "__main__":
    main()

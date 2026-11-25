#!/usr/bin/env python3
"""Exercise projectile cancellation and cursed item removal through real commands."""
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function
from check_new_monsters import BUILD, ROOT


CHECKS = r'''
static int prompts;
static int aim_cancellations;
static bool accept;
static bool cancel_drop_quantity, cancel_drop_confirmation;
static bool trace_bow;
static int release_cues, release_delays, flight_steps;
enum { ROW = 10, PLAYER_X = 32, FRONT_X = 31, TARGET_X = 30 };

void __real_sound(int effect);
void __wrap_sound(int effect)
{
    if (trace_bow && effect == MSG_SHOOT)
        release_cues++;
    __real_sound(effect);
}

errr __real_Term_xtra(int action, int value);
errr __wrap_Term_xtra(int action, int value)
{
    if (trace_bow && action == TERM_XTRA_DELAY) {
        if (value == 350) {
            assert(release_cues == 1 && flight_steps == 0);
            release_delays++;
        } else if (value == op_ptr->delay_factor * op_ptr->delay_factor) {
            assert(release_cues == 1 && release_delays == (use_sound ? 1 : 0));
            flight_steps++;
        }
    }
    return __real_Term_xtra(action, value);
}

bool __wrap_get_check_oath_multiline(cptr prompt)
{
    assert(prompt && prompt[0]);
    prompts++;
    return accept;
}

bool __wrap_target_select_aim(int range, bool allow_vertical, int* dir)
{
    assert(range > 0 && !allow_vertical && dir);
    aim_cancellations++;
    return false;
}

s16b __wrap_get_quantity_action(cptr prompt, cptr action, int max)
{
    (void)prompt;
    return cancel_drop_quantity && streq(action, "Drop") ? 0 : max;
}

bool __wrap_get_check(cptr prompt)
{
    return !cancel_drop_confirmation || strncmp(prompt, "Drop ", 5) != 0;
}

static monster_type* setup(bool throwing, bool front, bool radiant)
{
    monster_type* target = combat_fixture(82, 100); /* Easterling warrior */
    int target_idx = cave_m_idx[target->fy][target->fx];
    cave_m_idx[target->fy][target->fx] = 0;
    target->fx = TARGET_X;
    cave_m_idx[ROW][TARGET_X] = target_idx;
    p_ptr->px = PLAYER_X;
    cave_m_idx[ROW][PLAYER_X] = -1;
    p_ptr->song1 = p_ptr->song2 = SNG_NOTHING;
    p_ptr->oath_type = OATH_MERCY;
    p_ptr->stat_use[A_STR] = 5;
    player_pack_action_reset();
    player_carried_extra_reset_store();
    player_quiver_reset_store();

    for (int x = 1; x <= p_ptr->px; x++)
        cave_info[ROW][x] = CAVE_FIRE | CAVE_VIEW;

    if (front)
    {
        /* The front enemy is exempt from Mercy and can stop the projectile. */
        assert(place_monster_one(ROW, FRONT_X, 15, false, true, NULL));
        monster_type* other = &mon_list[cave_m_idx[ROW][FRONT_X]];
        other->ml = true;
        other->alertness = ALERTNESS_ALERT;
        other->skip_next_turn = true;
        other->hp = other->maxhp = 30000;
    }
    target->skip_next_turn = true;
    target->hp = target->maxhp = 30000;
    assert(target->ml && merciless_attack(target));

    if (throwing)
    {
        object_type* spear = &inventory[INVEN_WIELD];
        object_prep(spear, lookup_kind(TV_POLEARM, SV_SPEAR));
        p_ptr->equip_cnt = 1;
        assert(player_ready_throwing_weapon(spear, INVEN_WIELD));
    }
    else
    {
        object_type* bow = &inventory[INVEN_BOW];
        object_prep(bow, lookup_kind(TV_BOW, SV_SHORT_BOW));
        if (radiant)
            object_set_ego_prefix(bow, 93); /* (Radiant) */
        p_ptr->equip_cnt = 1;
        assert(player_ready_bow_with_arrow(-1));
        object_type arrows;
        object_prep(&arrows, lookup_kind(TV_ARROW, SV_NORMAL_ARROW));
        arrows.number = 4;
        assert(player_quiver_absorb_arrow(&arrows) == 4);
        p_ptr->ammo_tval = TV_ARROW;
    }

    p_ptr->energy_use = 0;
    p_ptr->previous_action[0] = ACTION_NOTHING;
    p_ptr->killed_enemy_with_arrow = true;
    p_ptr->command_dir = 4;
    repeat_clear();
    player_attacked = false;
    prompts = 0;
    aim_cancellations = 0;
    accept = false;
    cancel_drop_quantity = cancel_drop_confirmation = false;
    return target;
}

static void shoot(bool throwing, int skill)
{
    p_ptr->skill_base[S_ARC] = p_ptr->skill_use[S_ARC] = skill;
    p_ptr->skill_base[S_MEL] = p_ptr->skill_use[S_MEL] = skill;
    if (throwing)
        do_cmd_throw_from_slot(INVEN_WIELD);
    else
        do_cmd_fire(1);
}

static void check_refusal(bool throwing)
{
    monster_type* target = setup(throwing, false, true);
    shoot(throwing, -100);
    printf("%s first foe: prompts=%d energy=%d.\n", throwing ? "Throw" : "Bow", prompts, p_ptr->energy_use);
    assert(prompts == 1 && p_ptr->energy_use == 0);
    assert(!player_attacked && p_ptr->previous_action[0] == ACTION_NOTHING);
    assert(p_ptr->killed_enemy_with_arrow && target->hp == 30000 && o_cnt == 0);
    assert(!(cave_info[ROW][FRONT_X] & CAVE_GLOW) && !oath_invalid(OATH_MERCY));
    if (throwing) assert(inventory[INVEN_WIELD].number == 1);
    else assert(player_quiver_arrow_count() == 4);
}

static void check_peaceful_screen(bool throwing)
{
    setup(throwing, false, false);
    assert(place_monster_one(ROW, FRONT_X, R_IDX_HUMAN_THRALL, false, true, NULL));
    monster_type* thrall = &mon_list[cave_m_idx[ROW][FRONT_X]];
    thrall->ml = true;
    assert(r_info[thrall->r_idx].flags1 & RF1_PEACEFUL);
    shoot(throwing, -100);
    assert(prompts == 1 && !p_ptr->energy_use && !player_attacked && !o_cnt);
    if (throwing) assert(inventory[INVEN_WIELD].number == 1);
    else assert(player_quiver_arrow_count() == 4);
}

static void check_valour_refusal(bool throwing)
{
    monster_type* target = setup(throwing, false, false);
    p_ptr->oath_type = OATH_VALOROUS;
    target->stance = STANCE_FLEEING;
    shoot(throwing, -100);
    assert(prompts == 1 && !p_ptr->energy_use && !player_attacked && !o_cnt);
    if (throwing) assert(inventory[INVEN_WIELD].number == 1);
    else assert(player_quiver_arrow_count() == 4);
}

static object_type* curse_active_spear(bool can_break)
{
    object_type* spear = &inventory[INVEN_WIELD];
    spear->ident |= IDENT_CURSED;
    spear->discount = INSCRIP_CURSED;
    p_ptr->innate_ability[S_WIL][WIL_CURSE_BREAKING] = can_break;
    p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING] = can_break;
    assert(player_equipment_slot_counts_as_equipped(INVEN_WIELD));
    return spear;
}

static void check_cursed_throw(void)
{
    for (int cancel = 0; cancel < 3; ++cancel)
    {
        setup(true, false, false);
        object_type* spear = curse_active_spear(cancel != 0);
        if (cancel < 2)
            p_ptr->command_dir = 0;
        shoot(true, -100);
        printf("Cursed throw stopped (%d): cursed=%d aim=%d Mercy=%d energy=%d.\n",
            cancel, !!cursed_p(spear), aim_cancellations, prompts, p_ptr->energy_use);
        assert(cursed_p(spear) && !(spear->ident & IDENT_UNCURSED));
        assert(spear->discount == INSCRIP_CURSED && spear->number == 1);
        assert(!player_attacked && !p_ptr->energy_use && !o_cnt);
        assert(aim_cancellations == (cancel == 1) && prompts == (cancel == 2));
    }

    monster_type* target = setup(true, false, false);
    object_type* spear = curse_active_spear(true);
    p_ptr->oath_type = 0;
    shoot(true, 100);
    assert(!prompts && player_attacked && p_ptr->energy_use == 100);
    assert(!spear->k_idx);
    int y = target->fy, x = target->fx;
    int i = cave_o_idx[y][x];
    assert(i > 0 && o_list[i].tval == TV_POLEARM);
    assert(!cursed_p(&o_list[i]) && (o_list[i].ident & IDENT_UNCURSED));
    assert(o_list[i].discount != INSCRIP_CURSED && o_list[i].pickup
        && o_list[i].pickup_slot == PICKUP_SLOT_ACTIVE_THROWING);
    delete_monster_idx((int)(target - mon_list));
    cave_m_idx[p_ptr->py][p_ptr->px] = 0;
    p_ptr->py = y;
    p_ptr->px = x;
    cave_m_idx[y][x] = -1;
    py_pickup_aux(i);
    assert(!o_list[i].k_idx && spear->number == 1);
    assert(!cursed_p(spear) && (spear->ident & IDENT_UNCURSED));
    puts("Curse Breaking: refused and cancelled throws leave the curse; a committed throw and recovery preserve the uncursed spear: PASS.");
}

static object_type* bound_crown(void)
{
    setup(false, false, false);
    object_type* crown = &inventory[INVEN_HEAD];
    assert(make_fake_artefact(crown, ART_FEANOR));
    p_ptr->equip_cnt++;
    p_ptr->innate_ability[S_WIL][WIL_CURSE_BREAKING] = true;
    p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING] = true;
    assert(cursed_p(crown));
    return crown;
}

static void finish_pack_action(void)
{
    for (int i = 0; i < 3 && player_pack_action_pending(); i++)
        player_pack_action_process();
    assert(!player_pack_action_pending());
}

static void check_permanent_curse(void)
{
    object_type* crown = bound_crown();
    do_cmd_takeoff(crown, INVEN_HEAD);
    finish_pack_action();
    printf("Fëanor's crown takeoff: equipped=%d cursed=%d energy=%d.\n",
        !!crown->k_idx, !!cursed_p(crown), p_ptr->energy_use);
    assert(crown->k_idx && cursed_p(crown) && !p_ptr->energy_use);
    assert(!player_pack_action_pending());

    crown = bound_crown();
    bool dropped = do_cmd_drop_item_by_index_confirm(INVEN_HEAD, true);
    finish_pack_action();
    printf("Fëanor's crown drop: dropped=%d equipped=%d cursed=%d.\n",
        dropped, !!crown->k_idx, !!cursed_p(crown));
    assert(!dropped && crown->k_idx && cursed_p(crown) && !p_ptr->energy_use && !o_cnt);

    setup(true, false, false);
    object_type* spear = curse_active_spear(true);
    p_ptr->oath_type = 0;
    object_kind* spear_kind = &k_info[spear->k_idx];
    u32b old_flags = spear_kind->flags3;
    spear_kind->flags3 |= TR3_PERMA_CURSE;
    shoot(true, 100);
    spear_kind->flags3 = old_flags;
    assert(spear->k_idx && cursed_p(spear) && !p_ptr->energy_use && !o_cnt);
    assert(!player_attacked);

    crown = bound_crown();
    object_type* light = &inventory[0];
    object_prep(light, lookup_kind(TV_LIGHT, SV_LIGHT_SILMARIL));
    p_ptr->inven_cnt = 1;
    assert(do_cmd_wield_to_slot(light, 0, INVEN_LITE));
    finish_pack_action();
    assert(inventory[INVEN_LITE].sval == SV_LIGHT_SILMARIL);
    assert(crown->k_idx && !cursed_p(crown));
    p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING] = false;
    do_cmd_takeoff(crown, INVEN_HEAD);
    finish_pack_action();
    assert(!crown->k_idx);

    setup(false, false, false);
    object_type* bow = &inventory[INVEN_BOW];
    assert(make_fake_artefact(bow, 142)); /* Shortbow of Celegorm */
    assert(player_equipment_slot_counts_as_equipped(INVEN_BOW) && cursed_p(bow));
    assert(player_set_active_weapon_mode(PLAYER_ACTIVE_WEAPON_MELEE, false, false));
    assert(!player_equipment_slot_counts_as_equipped(INVEN_BOW));
    do_cmd_takeoff(bow, INVEN_BOW);
    finish_pack_action();
    assert(!bow->k_idx && player_pack_entry_count() == 1);
    object_type* stored = player_pack_entry_at(0);
    u32b f1, f2, f3;
    object_flags(stored, &f1, &f2, &f3);
    assert(stored->name1 == 142 && cursed_p(stored) && (f3 & TR3_PERMA_CURSE));
    puts("Fëanor's Oath: takeoff, drop and throw respect the active binding. Holy light frees the crown and inactive storage keeps Celegorm's curse: PASS.");
}

static void check_ordinary_cursed_drop(void)
{
    setup(true, false, false);
    object_type* spear = curse_active_spear(true);
    cancel_drop_quantity = true;
    assert(!do_cmd_drop_item_by_index_confirm(INVEN_WIELD, true));
    assert(cursed_p(spear) && !p_ptr->energy_use && !o_cnt);
    cancel_drop_quantity = false;
    cancel_drop_confirmation = true;
    assert(!do_cmd_drop_item_by_index_confirm(INVEN_WIELD, true));
    assert(cursed_p(spear) && !p_ptr->energy_use && !o_cnt);

    object_kind* kind = &k_info[spear->k_idx];
    u32b old_flags = kind->flags3;
    kind->flags3 |= TR3_HEAVY_CURSE;
    cancel_drop_confirmation = false;
    assert(do_cmd_drop_item_by_index_confirm(INVEN_WIELD, true));
    assert(!spear->k_idx && p_ptr->energy_use == 50);
    int idx = cave_o_idx[p_ptr->py][p_ptr->px];
    assert(idx > 0 && !cursed_p(&o_list[idx]) && (o_list[idx].ident & IDENT_UNCURSED));
    kind->flags3 = old_flags;
    puts("Dropping a cursed item may be cancelled at quantity or confirmation. A committed drop still releases a heavy curse: PASS.");
}

static void check_bystander(bool throwing, bool stops)
{
    monster_type* target = setup(throwing, true, false);
    monster_type* front = &mon_list[cave_m_idx[ROW][FRONT_X]];
    shoot(throwing, stops ? 100 : -100);
    assert(prompts == (stops ? 0 : 1));
    assert(player_attacked && p_ptr->energy_use == 100 && combat_number == 1);
    assert(target->hp == 30000 && !oath_invalid(OATH_MERCY));
    assert((front->hp < 30000) == stops);
    if (throwing)
    {
        assert(!inventory[INVEN_WIELD].k_idx);
        int slot = cave_o_idx[ROW][FRONT_X];
        assert(slot > 0 && o_list[slot].tval == TV_POLEARM);
    }
    else assert(player_quiver_arrow_count() == 3);
}

static void check_acceptance(bool sound_enabled)
{
    monster_type* target = setup(false, false, false);
    assert(target_able((int)(target - mon_list)));
    target_set_monster((int)(target - mon_list));
    p_ptr->command_dir = 5;
    accept = true;
    bool previous_sound = use_sound;
    int previous_delay = op_ptr->delay_factor;
    use_sound = sound_enabled;
    op_ptr->delay_factor = 3;
    release_cues = release_delays = flight_steps = 0;
    trace_bow = true;
    shoot(false, -100);
    trace_bow = false;
    use_sound = previous_sound;
    op_ptr->delay_factor = previous_delay;
    assert(release_cues == 1 && release_delays == (sound_enabled ? 1 : 0) && flight_steps > 0);
    assert(prompts == 1 && p_ptr->energy_use == 100 && player_attacked);
    assert(target->hp == 30000 && player_quiver_arrow_count() == 3);
}

static void check_zero_path(void)
{
    setup(false, false, false);
    for (int x = 0; x < p_ptr->px; x++)
        cave_info[ROW][x] &= ~CAVE_FIRE;
    int y = ROW, x = p_ptr->px - archery_range(&inventory[INVEN_BOW]);
    u16b path[256];
    assert(project_path(path, archery_range(&inventory[INVEN_BOW]),
        p_ptr->py, p_ptr->px, &y, &x, PROJECT_THRU) == 0);
    shoot(false, 100);
    assert(!prompts && !p_ptr->energy_use && !player_attacked);
    assert(player_quiver_arrow_count() == 4 && o_cnt == 0);
}

static void check_near_boundary(bool throwing)
{
    monster_type* target = setup(throwing, false, false);
    int target_idx = cave_m_idx[target->fy][target->fx];
    cave_m_idx[target->fy][target->fx] = 0;
    cave_m_idx[p_ptr->py][p_ptr->px] = 0;
    p_ptr->oath_type = 0;
    if (throwing)
    {
        /* Northwest with the west boundary closer than the north boundary. */
        p_ptr->py = 20;
        p_ptr->px = 3;
        p_ptr->command_dir = 7;
        target->fy = 19;
        target->fx = 2;
        for (int y = 10; y < 20; y++)
            for (int x = 0; x < 4; x++)
                cave_info[y][x] = CAVE_FIRE | CAVE_VIEW;
    }
    else
    {
        /* West from close enough to the edge that an unclamped aim leaves the map. */
        p_ptr->px = 12;
        p_ptr->command_dir = 4;
        target->fx = 10;
    }
    cave_m_idx[p_ptr->py][p_ptr->px] = -1;
    cave_m_idx[target->fy][target->fx] = target_idx;
    shoot(throwing, 100);
    assert(!prompts && p_ptr->energy_use == 100 && player_attacked);
    assert(target->hp < 30000 && combat_number == 1);
}

static void checks(void)
{
    for (int throwing = 0; throwing <= 1; throwing++)
    {
        check_refusal(throwing);
        check_peaceful_screen(throwing);
        check_valour_refusal(throwing);
        check_bystander(throwing, true);
        check_bystander(throwing, false);
        check_near_boundary(throwing);
    }
    check_acceptance(false);
    check_acceptance(true);
    puts("Bow release precedes flight, with the 350 ms lead-in only when sound is enabled: PASS.");
    check_zero_path();
    check_cursed_throw();
    check_ordinary_cursed_drop();
    check_permanent_curse();
    puts("Ranged commands: early and in-flight refusal, intervening hits, one confirmation, zero-path and boundary aim: PASS.");
}
'''


def main():
    out = ROOT / "scripts/output/player-ranged-cancellation"
    out.mkdir(parents=True, exist_ok=True)
    fixture = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    for name in ("terminal_extra", "reset_map", "combat_fixture"):
        fixture += fixture_function(name) + "\n"
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    fixture += CHECKS + init[:init.index("    check_templates();")]
    fixture += "    checks(); SDL_Quit(); return 0;\n}\n"
    source = out / "check.c"
    source.write_text(fixture, encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    replacements = ("src/cmd/combat/cmd-ranged.c", "src/cmd/item/cmd-item-core.c")
    excluded = tuple(f"/{part}.obj" for part in (*replacements, "src/main.c"))
    response = out / "objects.rsp"
    response.write_text("\n".join(f'"{obj}"' for obj in objects
        if not obj.endswith(excluded)), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / part) for part in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"],
    ])
    exe = out / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
        *(str(ROOT / part) for part in replacements), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-Wl,--wrap=get_check_oath_multiline",
        "-Wl,--wrap=target_select_aim", "-Wl,--wrap=get_quantity_action",
        "-Wl,--wrap=get_check", "-Wl,--wrap=sound", "-Wl,--wrap=Term_xtra",
        "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="data-", dir=out) as data:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), data], cwd=data, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()

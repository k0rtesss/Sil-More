#!/usr/bin/env python3
"""Exercise lantern equip and refuelling against production oil accounting."""
import argparse
import os
import shlex
import subprocess
import tempfile

from check_new_monsters import BUILD, HARNESS, ROOT

OUT = ROOT / "scripts/output/lantern-oil-transactions"
UNITS = ("src/cmd/item/cmd-item-core.c", "src/supplies.c")

CHECKS = r'''
static bool accept_overflow;
static int overflow_prompts;

bool __wrap_get_check(cptr prompt)
{
    assert(streq(prompt, "Refueling this lamp will waste some oil. Proceed? "));
    overflow_prompts++;
    return accept_overflow;
}

void __wrap_message_flush(void) {}

static void prepare(void)
{
    reset_map(5);
    memset(inventory, 0, INVEN_TOTAL * sizeof(*inventory));
    player_carried_extra_reset_store();
    supplies_reset_store();
    player_pack_action_reset();
    p_ptr->equip_cnt = p_ptr->inven_cnt = 0;
    p_ptr->energy_use = p_ptr->lamp_oil = 0;
    p_ptr->notice = p_ptr->update = p_ptr->redraw = p_ptr->window = 0;
    p_ptr->previous_action[0] = ACTION_NOTHING;
    p_ptr->mhp = p_ptr->chp = 50;
    p_ptr->msp = p_ptr->csp = 40;
    p_ptr->stat_use[A_STR] = p_ptr->stat_base[A_STR] = 5;
    p_ptr->total_weight = 0;
    cave_m_idx[p_ptr->py][p_ptr->px] = -1;
    playerturn = 1;
    overflow_prompts = 0;
}

static object_type light(int sval, int fuel)
{
    object_type obj;
    int kind = lookup_kind(TV_LIGHT, sval);
    assert(kind);
    object_prep(&obj, kind);
    object_known(&obj);
    obj.timeout = fuel;
    return obj;
}

static void wear_light(int sval, int fuel)
{
    inventory[INVEN_LITE] = light(sval, fuel);
    p_ptr->equip_cnt = 1;
}

static int put_underfoot(object_type* obj)
{
    int floor = floor_carry(p_ptr->py, p_ptr->px, obj);
    assert(floor > 0);
    return floor;
}

static void check_equip_oil(void)
{
    const char* source_names[] = { "Floor", "Supplies", "Legacy carried" };
    int failures = 0;
    for (int source = 0; source < (int)N_ELEMENTS(source_names); source++)
    {
        for (int spent = 0; spent < 2; spent++)
        {
            prepare();
            wear_light(SV_LIGHT_TORCH, spent ? 90 : 180);
            object_type lamp = light(SV_LIGHT_LANTERN, 3400);
            if (source == 1)
            {
                assert(supplies_absorb_object(&lamp) && player_lamp_oil() == 3400);
                supplies_begin_action(0);
                assert(do_cmd_wield_to_slot(supplies_entry_at(0), SUPPLIES_INDEX, INVEN_LITE));
                supplies_end_action();
            }
            else if (!source)
            {
                int floor = put_underfoot(&lamp);
                assert(do_cmd_wield_to_slot(&o_list[floor], -floor, INVEN_LITE));
                assert(!o_list[floor].k_idx);
            }
            else
            {
                inventory[0] = lamp;
                p_ptr->inven_cnt = 1;
                assert(do_cmd_wield_to_slot(&inventory[0], 0, INVEN_LITE));
                assert(!player_pack_entry_count());
            }
            assert(inventory[INVEN_LITE].sval == SV_LIGHT_LANTERN);
            assert(!inventory[INVEN_LITE].timeout && p_ptr->energy_use == 100);
            assert(player_lamp_oil_capacity() == FUEL_LAMP && supplies_entry_count() == !spent);
            int actual = player_lamp_oil();
            if (actual != 3400)
            {
                fprintf(stderr, "%s lantern replacing a %s torch retained %d/3400 fuel.\n",
                    source_names[source], spent ? "spent" : "usable", actual);
                failures++;
            }
        }
    }
    assert(!failures);
    puts("Equipping floor, Supplies, or legacy carried lanterns preserves all fuel when replacing usable and spent torches PASS.");
}

static void check_refuel_split(void)
{
    for (int constrained = 0; constrained < 2; constrained++)
    {
        prepare();
        wear_light(SV_LIGHT_LANTERN, 0);
        player_set_lamp_oil(1000);
        object_type lamp = light(SV_LIGHT_LANTERN, 1600);
        lamp.number = 2;
        int floor = put_underfoot(&lamp);
        int old_limit = z_info->o_max;
        if (constrained)
            z_info->o_max = o_max;
        do_cmd_refuel_lamp(&o_list[floor], -floor);
        z_info->o_max = old_limit;

        int oil = player_lamp_oil();
        int lanterns = 0, floor_oil = 0;
        for (int i = 1; i < o_max; i++)
        {
            if (o_list[i].k_idx && o_list[i].tval == TV_LIGHT)
            {
                lanterns += o_list[i].number;
                floor_oil += o_list[i].number * o_list[i].timeout;
            }
        }
        if (lanterns != 2)
            fprintf(stderr, "Refuel left %d of two floor lanterns with constrained=%d.\n",
                lanterns, constrained);
        assert(lanterns == 2 && oil + floor_oil == 4200);
        assert(oil == (constrained ? 1000 : 2600));
        assert(p_ptr->energy_use == (constrained ? 0 : 100));
        assert(o_list[floor].number == (constrained ? 2 : 1));
        assert(o_list[floor].timeout == 1600 && !overflow_prompts);
    }
    puts("Floor lantern-stack refuel conserves oil and containers, or refuses when there is nowhere for the emptied lamp PASS.");
}

static void check_carried_refuel(void)
{
    int failures = 0;
    build_randart_tables();
    for (int constrained = 0; constrained < 2; constrained++)
    {
        prepare();
        character_generated = character_dungeon = true;
        p_ptr->playing = true;
        p_ptr->max_depth = p_ptr->depth;
        p_ptr->food = PY_FOOD_FULL;
        turn = playerturn = 100;
        SDL_strlcpy(op_ptr->full_name, "legacy oil fixture", sizeof(op_ptr->full_name));
        strnfmt(savefile, sizeof(savefile), "legacy-oil-%d", constrained);

        wear_light(SV_LIGHT_LANTERN, 0);
        inventory[0] = light(SV_LIGHT_LANTERN, 1600);
        inventory[0].number = 2;
        p_ptr->inven_cnt = 1;
        player_set_lamp_oil(1000);

        object_type protected_floor;
        const artefact_type* artifact = &a_info[ART_ORCRIST];
        object_prep(&protected_floor, lookup_kind(artifact->tval, artifact->sval));
        protected_floor.name1 = ART_ORCRIST;
        assert(put_underfoot(&protected_floor) > 0);

        assert(save_player() && load_player() && character_loaded && character_dungeon);
        assert(inventory[0].number == 2 && inventory[0].timeout == 1600);
        assert(player_oil_container_slots_used() > PLAYER_OIL_CONTAINER_SLOT_CAP);
        assert(player_lamp_oil() == 1000 && !supplies_entry_count());
        int floor = cave_o_idx[p_ptr->py][p_ptr->px];
        assert(floor > 0 && o_list[floor].name1 == ART_ORCRIST && o_cnt == 1);

        int old_limit = z_info->o_max;
        if (constrained)
            z_info->o_max = o_max;
        p_ptr->energy_use = 0;
        p_ptr->previous_action[0] = ACTION_NOTHING;
        do_cmd_refuel_lamp(&inventory[0], 0);
        z_info->o_max = old_limit;

        assert(o_list[floor].name1 == ART_ORCRIST && !overflow_prompts);
        assert(inventory[0].timeout == 1600 && !supplies_entry_count());
        if (constrained)
        {
            if (inventory[0].number != 2 || player_lamp_oil() != 1000 || p_ptr->energy_use)
            {
                fprintf(stderr, "Blocked carried refuel left %d donors, %d oil and %d energy.\n",
                    inventory[0].number, player_lamp_oil(), p_ptr->energy_use);
                failures++;
            }
            else
            {
                assert(p_ptr->previous_action[0] == ACTION_NOTHING && o_cnt == 1);
            }
        }
        else
        {
            assert(inventory[0].number == 1 && o_cnt == 2);
            assert(player_lamp_oil() == 2600 && p_ptr->energy_use == 100);
            for (int i = 1; i < o_max; i++)
                if (o_list[i].k_idx && i != floor)
                    assert(o_list[i].sval == SV_LIGHT_LANTERN && !o_list[i].timeout);
            p_ptr->energy_use = 0;
            do_cmd_refuel_lamp(&inventory[0], 0);
            assert(inventory[0].number == 1 && !inventory[0].timeout && o_cnt == 2);
            assert(player_lamp_oil() == 4200 && p_ptr->energy_use == 100);
        }
    }
    assert(!failures);
    puts("Over-cap legacy inventory survives real save/reload. Refuel spills the empty unit when possible and aborts freely when blocked PASS.");
}

static void check_controls(void)
{
    prepare();
    wear_light(SV_LIGHT_LANTERN, 0);
    player_set_lamp_oil(4700);
    cave_set_feat(p_ptr->py, p_ptr->px, FEAT_SUNLIGHT);
    object_type flask;
    object_prep(&flask, lookup_kind(TV_FLASK, 0));
    flask.pval = 800;
    int floor = put_underfoot(&flask);
    accept_overflow = false;
    do_cmd_refuel_lamp(&o_list[floor], -floor);
    assert(!p_ptr->energy_use && player_lamp_oil() == 4700 && o_list[floor].pval == 800);
    assert(overflow_prompts == 1);
    accept_overflow = true;
    do_cmd_refuel_lamp(&o_list[floor], -floor);
    assert(p_ptr->energy_use == 100 && player_lamp_oil() == FUEL_LAMP && !o_list[floor].k_idx);
    assert(overflow_prompts == 2);

    prepare();
    wear_light(SV_LIGHT_FEANORIAN, 0);
    floor = put_underfoot(&flask);
    do_cmd_refuel_lamp(&o_list[floor], -floor);
    assert(!p_ptr->energy_use && !player_lamp_oil() && !overflow_prompts);
    assert(o_list[floor].pval == 800 && inventory[INVEN_LITE].sval == SV_LIGHT_FEANORIAN);
    assert(!player_light_uses_oil_pool(&inventory[INVEN_LITE]));

    prepare();
    wear_light(SV_LIGHT_LANTERN, 0);
    object_type lamp = light(SV_LIGHT_LANTERN, 0);
    floor = put_underfoot(&lamp);
    int old_cap = z_info->o_max;
    z_info->o_max = o_max;
    do_cmd_refuel_lamp(&o_list[floor], -floor);
    assert(!p_ptr->energy_use && !player_lamp_oil() && o_list[floor].number == 1);
    o_list[floor].timeout = 1600;
    do_cmd_refuel_lamp(&o_list[floor], -floor);
    z_info->o_max = old_cap;
    assert(p_ptr->energy_use == 100 && player_lamp_oil() == 1600);
    assert(o_list[floor].number == 1 && !o_list[floor].timeout && !overflow_prompts);
    puts("Daylight refuel prompts before intentional overflow; cancel is free and permanent lights reject oil PASS.");
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", action="store_true")
    args = parser.parse_args()
    out = OUT / ("baseline" if args.baseline else "current")
    out.mkdir(parents=True, exist_ok=True)
    prefix, init = HARNESS.split("int main(int argc,char** argv)", 1)
    init = init.split("    check_templates();", 1)[0].replace("argc==3", "argc==4")
    source = out / "check.c"
    source.write_text(prefix + CHECKS + "int main(int argc,char** argv)" + init +
                      "    assert(init_flavor_info()==0); flavor_init();\n"
                      "    ANGBAND_DIR_SAVE = ANGBAND_DIR_APEX = argv[2];\n"
                      '    if (streq(argv[3], "equip")) check_equip_oil();\n'
                      '    if (streq(argv[3], "split")) check_refuel_split();\n'
                      '    if (streq(argv[3], "carried")) check_carried_refuel();\n'
                      '    if (streq(argv[3], "controls")) check_controls();\n'
                      "    SDL_Quit(); return 0;\n}\n", encoding="utf-8")
    compiled = []
    for unit in UNITS:
        file = ROOT / unit
        if args.baseline:
            file = out / file.name
            file.write_bytes(subprocess.check_output(["git", "show", f"HEAD:{unit}"], cwd=ROOT))
        compiled.append(str(file))
    excluded = ("/src/main.c.obj", *(f"/{unit}.obj" for unit in UNITS))
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    response = out / "objects.rsp"
    response.write_text("\n".join(f'"{obj}"' for obj in objects if not obj.endswith(excluded)),
                        encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / dep) for dep in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"],
    ])
    exe = out / "check.exe"
    subprocess.run([
        "C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), *compiled,
        "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
        "-Wl,--wrap=get_check", "-Wl,--wrap=message_flush", "-o", str(exe),
    ], cwd=BUILD, env=env, check=True)
    failed = []
    for case in ("equip", "split", "carried", "controls"):
        with tempfile.TemporaryDirectory(prefix="fixture-", dir=out) as state:
            result = subprocess.run([str(exe), str(ROOT / "lib/edit"), state, case],
                                    cwd=state, env=env, timeout=20)
            if result.returncode:
                failed.append(f"{case} (exit {result.returncode})")
    if failed:
        raise SystemExit(f"Failed cases: {', '.join(failed)}")


if __name__ == "__main__":
    main()

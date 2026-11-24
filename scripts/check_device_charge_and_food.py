#!/usr/bin/env python3
"""Exercise empty staff activation, recharging, and last-herb autoinscription."""
import argparse
import os
import shlex
import subprocess
import tempfile

from check_new_monsters import BUILD, HARNESS, ROOT

OUT = ROOT / "scripts/output/device-charge-and-food"
ACTIVATION = "src/cmd/item/cmd-item-activate.c"

CHECKS = r'''
static int recharge_prompts, empty_messages, truce_calls;
static int inscription_prompts;
static const char* inscription_prompt;
static char recharge_key;
static char charge_message[96];
static int channel_prompts;
void fixture_staff_sample(object_type* staff, int kind);

void __wrap_msg_print(cptr message)
{
    if (message && streq(message, "The staff has no charges left."))
        empty_messages++;
}
void __wrap_msg_format(cptr fmt, ...)
{
    if (!strstr(fmt, "charge%s remaining."))
        return;

    va_list args;
    va_start(args, fmt);
    vsnprintf(charge_message, sizeof(charge_message), fmt, args);
    va_end(args);
}
void __wrap_handle_stuff(void) {}
void __wrap_message_flush(void) {}
void __wrap_break_truce(bool obvious) { (void)obvious; truce_calls++; }
bool __wrap_easter_time(void) { return false; }

bool __wrap_get_check(cptr prompt)
{
    if (!strncmp(prompt, "Channel ", 8))
    {
        channel_prompts++;
        assert(strstr(prompt, "(1 charges)?"));
        return true;
    }
    inscription_prompts++;
    assert(inscription_prompt && streq(prompt, inscription_prompt));
    return false;
}

char __wrap_inkey(void)
{
    assert(recharge_key && ++recharge_prompts == 1);
    return recharge_key;
}

static void reset_action(void)
{
    p_ptr->energy_use = 0;
    p_ptr->previous_action[0] = ACTION_NOTHING;
    empty_messages = truce_calls = 0;
}

static int prepare(object_type* object, int tval, int sval)
{
    int kind = lookup_kind(tval, sval);
    assert(kind);
    object_prep(object, kind);
    k_info[kind].aware = k_info[kind].tried = false;
    return kind;
}

static void prepare_staff(bool known)
{
    memset(inventory, 0, INVEN_TOTAL * sizeof(*inventory));
    int kind = prepare(&inventory[0], TV_STAFF, SV_STAFF_IMPRISONMENT);
    inventory[0].pval = CHANNELING_CHARGE_MULTIPLIER;
    if (known)
    {
        k_info[kind].aware = true;
        object_known(&inventory[0]);
    }
    assert(inventory_limit_group_for_object(&inventory[0]) == INV_LIMIT_HARNESS);
    assert(object_known_p(&inventory[0]) == known);
}

static void recharge_with_gem(char key)
{
    reset_action();
    recharge_key = key;
    recharge_prompts = 0;
    supplies_begin_action(0);
    do_cmd_use_gem(supplies_entry_at(0), SUPPLIES_INDEX);
    supplies_end_action();
    assert(recharge_prompts == 1);
}

static void check_staff(bool known)
{
    reset_map(5);
    prepare_staff(known);
    reset_action();
    do_cmd_activate_staff(NULL, 0);
    assert(p_ptr->energy_use == 100 && p_ptr->previous_action[0] == ACTION_MISC);
    assert(inventory[0].pval == 0 && inventory[0].xtra1 == 1 && truce_calls == 1);
    if (known)
    {
        char description[120];
        object_desc(description, sizeof(description), &inventory[0], true, 3);
        assert(strstr(description, "(0 charges)"));
    }

    reset_action();
    do_cmd_activate_staff(NULL, 0);
    assert(p_ptr->energy_use == (known ? 0 : 100));
    assert(p_ptr->previous_action[0] == (known ? ACTION_NOTHING : ACTION_MISC));
    assert(inventory[0].pval == 0 && inventory[0].xtra1 == 1);
    assert(empty_messages == 1 && truce_calls == 0);
    if (!known)
        assert(inventory[0].ident & IDENT_EMPTY);

    reset_action();
    do_cmd_activate_staff(NULL, 0);
    assert(!p_ptr->energy_use && empty_messages == 1 && !truce_calls);

    object_type donor;
    object_prep(&donor, lookup_kind(TV_STAFF, SV_STAFF_SLUMBER));
    donor.pval = CHANNELING_CHARGE_MULTIPLIER;
    int floor = floor_carry(p_ptr->py, p_ptr->px, &donor);
    assert(floor > 0);

    object_type gem;
    prepare(&gem, TV_GEM, SV_GEM_RECHARGING);
    gem.number = 2;
    assert(supplies_absorb_object(&gem));
    recharge_with_gem(ESCAPE);
    assert(!p_ptr->energy_use && !truce_calls && !inventory[0].pval);
    assert(o_list[floor].pval == donor.pval && supplies_entry_units(0) == 2);
    recharge_with_gem('a');
    assert(p_ptr->energy_use == 100 && inventory[0].pval == CHANNELING_CHARGE_MULTIPLIER);
    assert(supplies_entry_units(0) == 1);
    assert(!(inventory[0].ident & IDENT_EMPTY));
    assert(o_list[floor].pval == donor.pval);

    reset_action();
    do_cmd_activate_staff(NULL, 0);
    assert(p_ptr->energy_use == 100 && truce_calls == 1 && !empty_messages);
    assert(inventory[0].pval == 0 && inventory[0].xtra1 == 2);
    recharge_with_gem('-');
    assert(o_list[floor].pval == 2 * CHANNELING_CHARGE_MULTIPLIER);
    assert(!inventory[0].pval && supplies_entry_count() == 0 && p_ptr->energy_use == 100);
    puts(known ? "An identified empty staff refuses immediately and works after recharge PASS."
        : "An unidentified staff spends one discovery turn when empty and works after recharge PASS.");
}

static void check_odd_charges(void)
{
    reset_map(5);
    prepare_staff(true);
    object_type staff;
    object_copy(&staff, &inventory[0]);
    int floor = floor_carry(p_ptr->py, p_ptr->px, &staff);
    assert(floor > 0);

    int mismatches = 0;
    for (int usable = 0; usable <= 1; usable++)
    {
        int stored = 2 * usable + 1;
        inventory[0].pval = o_list[floor].pval = stored;
        inventory[0].ident &= ~IDENT_EMPTY;
        char suffix[32];
        char description[120];
        char expected[96];
        const char* plural = usable == 1 ? "" : "s";
        strnfmt(suffix, sizeof(suffix), "(%d charge%s)", usable, plural);
        object_desc(description, sizeof(description), &inventory[0], true, 3);
        mismatches += !strstr(description, suffix);

        charge_message[0] = '\0';
        inven_item_charges(0);
        strnfmt(expected, sizeof(expected), "You have %d charge%s remaining.", usable, plural);
        mismatches += !streq(charge_message, expected);

        charge_message[0] = '\0';
        floor_item_charges(floor);
        strnfmt(expected, sizeof(expected), "There are %d charge%s remaining.", usable, plural);
        mismatches += !streq(charge_message, expected);

        reset_action();
        do_cmd_activate_staff(NULL, 0);
        assert(inventory[0].pval == 1);
        if (usable)
            assert(p_ptr->energy_use == 100);
        assert(truce_calls == usable && empty_messages == 1 - usable);
    }
    if (mismatches)
        fprintf(stderr, "%d charge displays disagreed with staff activation.\n", mismatches);
    assert(!mismatches);
    char description[120];
    fixture_staff_sample(&staff, inventory[0].k_idx);
    object_desc(description, sizeof(description), &staff, true, 3);
    assert(strstr(description, "(1 charge)"));

    p_ptr->active_ability[S_WIL][WIL_CHANNELING] = true;
    reset_action();
    o_list[floor].pval = 1;
    channel_prompts = 0;
    assert(!player_channel_floor_staff(&o_list[floor], floor));
    assert(!channel_prompts && !p_ptr->energy_use && inventory[0].pval == 1);
    o_list[floor].pval = 3;
    assert(player_channel_floor_staff(&o_list[floor], floor));
    assert(channel_prompts == 1 && p_ptr->energy_use == 100 && inventory[0].pval == 2);
    assert(!o_list[floor].k_idx);

    inventory[0].pval = 1;
    object_type gem;
    prepare(&gem, TV_GEM, SV_GEM_RECHARGING);
    gem.number = 2;
    assert(supplies_absorb_object(&gem));
    p_ptr->active_ability[S_WIL][WIL_CHANNELING] = false;
    recharge_with_gem('a');
    assert(inventory[0].pval == 3 && p_ptr->energy_use == 100);
    p_ptr->active_ability[S_WIL][WIL_CHANNELING] = true;
    recharge_with_gem('a');
    assert(inventory[0].pval == 7 && !supplies_entry_count());
    puts("Odd raw charge counts display only whole uses; activation, channeling and recharge preserve their costs PASS.");
}

static void check_herb(bool from_floor)
{
    reset_map(5);
    p_ptr->food = 3000;
    memset(p_ptr->stat_drain, 0, sizeof(p_ptr->stat_drain));
    object_type herb, food;
    int herb_kind = prepare(&herb, TV_FOOD, SV_FOOD_RESTORATION);
    int food_kind = prepare(&food, TV_FOOD, SV_FOOD_LEMBAS);
    assert(!object_aware_p(&herb));

    inscription_prompts = 0;
    inscription_prompt = "Autoinscribe this herb type? ";
    if (from_floor)
    {
        int floor = floor_carry(p_ptr->py, p_ptr->px, &herb);
        assert(floor > 0);
        do_cmd_eat_food(&o_list[floor], -floor);
        assert(!o_list[floor].k_idx && supplies_entry_count() == 0);
    }
    else
    {
        assert(supplies_absorb_object(&herb) && supplies_absorb_object(&food));
        assert(supplies_entry_at(0)->k_idx == herb_kind);
        supplies_begin_action(0);
        do_cmd_eat_food(supplies_entry_at(0), SUPPLIES_INDEX);
        supplies_end_action();
        assert(supplies_entry_count() == 1 && supplies_entry_at(0)->k_idx == food_kind);
        assert(supplies_entry_units(0) == 1);
    }
    assert(p_ptr->energy_use == 100 && k_info[herb_kind].tried && !k_info[herb_kind].aware);
    assert(p_ptr->food == 3000 + k_info[herb_kind].pval && inscription_prompts == 1);
    puts(from_floor ? "Eating a last unknown floor herb offers the herb autoinscription PASS."
        : "Eating a last Supplies herb offers its autoinscription even when the next entry is food PASS.");
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", action="store_true")
    cases = ("known-staff", "unknown-staff", "odd-charges", "supplies-herb", "floor-herb")
    parser.add_argument("--case", choices=cases)
    args = parser.parse_args()
    out = OUT / ("baseline" if args.baseline else "current")
    out.mkdir(parents=True, exist_ok=True)
    prefix, init = HARNESS.split("int main(int argc,char** argv)", 1)
    init = init.split("    check_templates();", 1)[0].replace("argc==3", "argc==4")
    source = out / "check.c"
    source.write_text(prefix + CHECKS + "int main(int argc,char** argv)" + init +
                      "    assert(init_flavor_info()==0); flavor_init();\n"
                      '    if (streq(argv[3], "known-staff")) check_staff(true);\n'
                      '    if (streq(argv[3], "unknown-staff")) check_staff(false);\n'
                      '    if (streq(argv[3], "odd-charges")) check_odd_charges();\n'
                      '    if (streq(argv[3], "supplies-herb")) check_herb(false);\n'
                      '    if (streq(argv[3], "floor-herb")) check_herb(true);\n'
                      "    SDL_Quit(); return 0;\n}\n", encoding="utf-8")
    units = (ACTIVATION, "src/object/object-desc.c", "src/object/object-inventory.c")
    compiled = []
    for unit in units:
        file = ROOT / unit
        if args.baseline:
            file = out / file.name
            file.write_bytes(subprocess.check_output(["git", "show", f"HEAD:{unit}"], cwd=ROOT))
        compiled.append(str(file))
    ui = "src/cmd/ui/cmd-ui-knowledge.c"
    sample = out / "sample.c"
    sample.write_text(f'#include "{(ROOT / ui).as_posix()}"\n'
                      "void fixture_staff_sample(object_type* staff, int kind) {\n"
                      "    knowledge_prepare_display_object(staff, kind);\n"
                      "    apply_magic_fake(staff); object_known(staff);\n"
                      "}\n", encoding="utf-8")
    excluded = ("/src/main.c.obj", f"/{ui}.obj", *(f"/{unit}.obj" for unit in units))
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    response = out / "objects.rsp"
    response.write_text("\n".join(f'"{obj}"' for obj in objects if not obj.endswith(excluded)),
                        encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / dep) for dep in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"],
    ])
    wraps = ("msg_print", "msg_format", "handle_stuff", "message_flush", "break_truce", "easter_time",
             "get_check", "inkey")
    exe = out / "check.exe"
    subprocess.run([
        "C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), str(sample), *compiled,
        "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
        *(f"-Wl,--wrap={name}" for name in wraps), "-o", str(exe),
    ], cwd=BUILD, env=env, check=True)
    failed = []
    for case in (args.case,) if args.case else cases:
        with tempfile.TemporaryDirectory(prefix="fixture-", dir=out) as state:
            result = subprocess.run([str(exe), str(ROOT / "lib/edit"), state, case],
                                    cwd=state, env=env, timeout=15)
            if result.returncode:
                failed.append(case)
    if failed:
        raise SystemExit(f"Failed cases: {', '.join(failed)}")


if __name__ == "__main__":
    main()

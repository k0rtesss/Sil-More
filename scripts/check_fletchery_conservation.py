#!/usr/bin/env python3
"""Check arrow conservation across Fletchery floor spills and bulk carving."""
import argparse
import os
import shlex
import subprocess
import tempfile

from check_new_monsters import BUILD, HARNESS, ROOT

OUT = ROOT / "scripts/output/fletchery-conservation"
FLETCHERY = "src/cmd/item/cmd-fletchery.c"

CHECKS = r'''
#include "object/object-internal.h"

static int selected_item, amount_to_convert;

bool __wrap_open_inventory_item_select_menu(int mode, cptr title, cptr none,
    int* selected)
{
    (void)none;
    assert(streq(title, "Choose fletchery source."));
    assert((mode & (USE_INVEN | USE_FLOOR | USE_QUIVER))
        == (USE_INVEN | USE_FLOOR | USE_QUIVER));
    if (selected_item != SUPPLIES_INDEX)
        assert(item_tester_hook(inventory_item_to_object_ptr(selected_item)));
    *selected = selected_item;
    item_tester_hook = NULL;
    return true;
}

s16b __wrap_get_quantity_action(cptr prompt, cptr action, int max)
{
    assert(streq(prompt, "Convert how many?") && streq(action, "Convert"));
    assert(amount_to_convert <= max);
    return amount_to_convert;
}

void __wrap_handle_stuff(void) {}
void __wrap_message_flush(void) {}

static object_type make(int tval, int sval, int quantity)
{
    object_type object;
    int kind = lookup_kind(tval, sval);

    assert(kind);
    object_prep(&object, kind);
    object.number = quantity;
    return object;
}

static object_type make_arrows(int quantity, int attack)
{
    object_type arrows = make(TV_ARROW, SV_NORMAL_ARROW, quantity);
    arrows.att = attack;
    return arrows;
}

static void prepare(void)
{
    reset_map(5);
    memset(inventory, 0, INVEN_TOTAL * sizeof(*inventory));
    player_carried_extra_reset_store();
    player_quiver_reset_store();
    supplies_reset_store();
    p_ptr->active_ability[S_ARC][ARC_FLETCHERY] = true;
    p_ptr->inven_cnt = p_ptr->equip_cnt = 0;
    p_ptr->energy_use = p_ptr->fletching = 0;
    p_ptr->fletch_item = -1;
    cave_m_idx[p_ptr->py][p_ptr->px] = -1;
}

static int pack_arrows(int attack)
{
    int total = 0;
    for (int i = 0; i < player_pack_entry_count(); i++)
    {
        const object_type* obj = player_pack_entry_at(i);
        if (obj->tval == TV_ARROW && obj->att == attack)
        {
            assert(obj->number <= object_stack_limit(obj));
            total += obj->number;
        }
    }
    return total;
}

static int floor_arrows(int attack)
{
    int total = 0;
    for (int i = 1; i < o_max; i++)
    {
        const object_type* obj = &o_list[i];
        if (obj->k_idx && obj->tval == TV_ARROW && obj->att == attack)
        {
            assert(obj->number <= object_stack_limit(obj));
            total += obj->number;
        }
    }
    return total;
}

static void check_spill(void)
{
    prepare();
    object_type arrows = make_arrows(48, 0);
    for (int n = inventory_limit_max_carryable_quantity(&arrows); n > 0;
         n = inventory_limit_max_carryable_quantity(&arrows))
    {
        arrows.number = n;
        assert(inven_carry(&arrows, false) >= 0 && !arrows.number);
        arrows = make_arrows(48, 0);
    }
    arrows = make_arrows(3, 3);
    assert(!inven_carry_okay(&arrows) && pack_arrows(0) > 0);
    int original_arrows = pack_arrows(0);

    arrows = make_arrows(47, 3);
    int floor = floor_carry(p_ptr->py, p_ptr->px, &arrows);
    assert(floor > 0);
    object_type torch = make(TV_LIGHT, SV_LIGHT_TORCH, 1);
    torch.timeout = 1000;
    assert(supplies_absorb_object(&torch));

    selected_item = SUPPLIES_INDEX;
    amount_to_convert = 1;
    do_cmd_fletchery();
    assert(p_ptr->energy_use == 100 && !supplies_entry_count());
    assert(pack_arrows(0) == original_arrows && !pack_arrows(3));
    int total = floor_arrows(3);
    if (total != 50)
        fprintf(stderr, "One torch added %d floor arrows, expected 3.\n", total - 47);
    assert(total == 50);
    puts("Full Pack with 47 compatible floor arrows: one torch adds exactly three arrows PASS.");
}

static void check_bulk(void)
{
    prepare();
    int floor = 0;
    for (int total = 0; total < 86;)
    {
        int amount = MIN(86 - total, PLAYER_TORCH_CAP);
        object_type torches = make(TV_LIGHT, SV_LIGHT_TORCH, amount);
        torches.timeout = 1000;
        object_known(&torches);
        floor = floor_carry(p_ptr->py, p_ptr->px, &torches);
        assert(floor > 0);
        total += amount;
    }
    assert(o_list[floor].number == 86);
    selected_item = -floor;
    amount_to_convert = 86;
    do_cmd_fletchery();
    assert(p_ptr->energy_use == 100);
    int total = pack_arrows(3) + floor_arrows(3);
    if (total != 258)
        fprintf(stderr, "Bulk carving delivered %d arrows, expected 258.\n", total);
    assert(total == 258);
    assert(pack_arrows(3) && floor_arrows(3));
    for (int i = 1; i < o_max; i++)
        assert(!o_list[i].k_idx || o_list[i].tval != TV_LIGHT);
    puts("Carving an 86-torch floor cache yields all 258 arrows in legal Pack and floor stacks PASS.");
}

static void check_controls(void)
{
    prepare();
    object_type staff = make(TV_STAFF, SV_STAFF_SLUMBER, 1);
    staff.pval = 4 * CHANNELING_CHARGE_MULTIPLIER;
    selected_item = inven_carry(&staff, false);
    assert(selected_item >= 0);
    amount_to_convert = 0;
    do_cmd_fletchery();
    assert(!p_ptr->energy_use && !pack_arrows(3) && player_pack_entry_count() == 1);
    amount_to_convert = 1;
    do_cmd_fletchery();
    assert(p_ptr->energy_use == 100 && pack_arrows(3) == 6 && !floor_arrows(3));
    assert(player_pack_entry_count() == 1);

    prepare();
    object_type arrows = make_arrows(8, 0);
    assert(player_quiver_absorb_arrow(&arrows) == 8);
    selected_item = QUIVER_INDEX;
    do_cmd_fletchery();
    assert(p_ptr->fletching == 8);
    p_ptr->fletching = 5;
    disturb(0, 0);
    assert(!p_ptr->fletching && player_quiver_arrow_count() == 8);
    assert(player_quiver_arrow_object(player_quiver_selected_arrow_slot())->att == 3);
    assert(player_quiver_arrow_object(player_quiver_selected_arrow_slot())->number == 3);
    assert(!pack_arrows(3) && !floor_arrows(3));
    puts("Cancelled carving is free, one staff yields six arrows, and interrupted Quiver work preserves all eight PASS.");
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
                      '    if (streq(argv[3], "spill")) check_spill();\n'
                      '    if (streq(argv[3], "bulk")) check_bulk();\n'
                      '    if (streq(argv[3], "controls")) check_controls();\n'
                      "    SDL_Quit(); return 0;\n}\n", encoding="utf-8")
    unit = ROOT / FLETCHERY
    if args.baseline:
        unit = out / unit.name
        unit.write_bytes(subprocess.check_output(["git", "show", f"HEAD:{FLETCHERY}"], cwd=ROOT))
    excluded = ("/src/main.c.obj", f"/{FLETCHERY}.obj")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    response = out / "objects.rsp"
    response.write_text("\n".join(f'"{obj}"' for obj in objects if not obj.endswith(excluded)),
                        encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / dep) for dep in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"],
    ])
    wraps = ("open_inventory_item_select_menu", "get_quantity_action", "handle_stuff", "message_flush")
    exe = out / "check.exe"
    subprocess.run([
        "C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), str(unit),
        "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
        *(f"-Wl,--wrap={name}" for name in wraps), "-o", str(exe),
    ], cwd=BUILD, env=env, check=True)
    failed = []
    for case in ("spill", "bulk", "controls"):
        with tempfile.TemporaryDirectory(prefix="fixture-", dir=out) as state:
            result = subprocess.run([str(exe), str(ROOT / "lib/edit"), state, case],
                                    cwd=state, env=env, timeout=20)
            if result.returncode:
                failed.append(f"{case} (exit {result.returncode})")
    if failed:
        raise SystemExit(f"Failed cases: {', '.join(failed)}")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Exercise arrow drops and the floor-to-Quiver command with a full, mixed quiver."""
import os
import shlex
import subprocess
import tempfile

from check_new_monsters import BUILD, HARNESS, ROOT

OUT = ROOT / "scripts/output/quiver-replacement"
CHECKS = r'''
#include "object/object-ui-select.h"

static bool cancel_second;
static int selections;

s16b __wrap_get_quantity_action(cptr prompt, cptr action, int max)
{
    assert(strstr(prompt, "Quiver") && streq(action, "Move") && max == 8);
    return 8;
}

bool __wrap_object_choice_overlay(cptr title, cptr desc,
    const object_choice_entry* entries, int count, int initial, int* selected)
{
    assert(streq(title, "What to replace?") && initial == 0);
    selections++;
    if (selections == 1)
    {
        assert(count == 2 && entries[0].o_ptr->att == 0);
        assert(strstr(desc, "48/48") && strstr(desc, "8 quivered arrows"));
    }
    else
    {
        assert(selections == 2 && count == 1 && entries[0].o_ptr->att == 1);
        assert(strstr(desc, "45/48") && strstr(desc, "5 quivered arrows"));
        if (cancel_second)
            return false;
    }
    *selected = 0;
    return true;
}

static object_type arrows(int attack, int count)
{
    object_type obj;

    object_prep(&obj, lookup_kind(TV_ARROW, SV_NORMAL_ARROW));
    obj.number = count;
    obj.att = attack;
    return obj;
}

static void check_arrow_drop_near_full_stack(void)
{
    reset_map(5);
    object_type floor_arrows = arrows(0, 47);
    object_type carried_arrows = arrows(0, 8);
    int pile = floor_carry(p_ptr->py, p_ptr->px, &floor_arrows);
    int slot = inven_carry(&carried_arrows, false);
    assert(pile > 0 && player_inventory_handle_is_carried(slot));
    inven_drop(slot, 8);

    int total = 0;
    for (int i = 1; i < o_max; i++)
        if (o_list[i].tval == TV_ARROW) total += o_list[i].number;
    assert(total == 55 && o_list[pile].number == 47 && o_cnt == 2);
    puts("Dropping eight arrows beside a stack of 47 preserves all 55: PASS");
}

static void check_replacement(bool cancel)
{
    reset_map(5);
    player_quiver_reset_store();
    p_ptr->stat_use[A_STR] = 3;
    p_ptr->energy_use = 0;
    p_ptr->previous_action[0] = ACTION_NOTHING;
    selections = 0;
    cancel_second = cancel;

    object_type first = arrows(0, 3);
    object_type second = arrows(1, 45);
    object_type incoming = arrows(2, 8);
    assert(player_quiver_absorb_arrow(&first) == 3);
    assert(player_quiver_absorb_arrow(&second) == 45);
    assert(player_quiver_select_arrow(QUIVER_INDEX + 1));
    int item = floor_carry(p_ptr->py, p_ptr->px, &incoming);
    assert(item > 0);

    bool moved = floor_context_perform_action(-item, FLOOR_CONTEXT_ACTION_QUIVER);
    assert(selections == 2);
    assert(player_quiver_store_entry_count() == 2);
    assert(player_quiver_arrow_count() == QUIVER_ARROW_CAPACITY);
    assert(moved == !cancel);
    if (cancel)
    {
        assert(player_quiver_selected_arrow_slot() == QUIVER_INDEX + 1);
        assert(player_quiver_store_entry_at(0)->number == 3);
        assert(player_quiver_store_entry_at(1)->number == 45);
        assert(o_cnt == 1 && o_list[item].number == 8);
        assert(p_ptr->energy_use == 0 && p_ptr->previous_action[0] == ACTION_NOTHING);
        puts("Cancel after selecting the first replacement preserves the quiver and floor PASS.");
    }
    else
    {
        object_type* retained = player_quiver_arrow_object(QUIVER_INDEX);
        object_type* added = player_quiver_arrow_object(QUIVER_INDEX + 1);
        assert(player_quiver_selected_arrow_slot() == QUIVER_INDEX);
        assert(retained->att == 1 && retained->number == 40);
        assert(added->att == 2 && added->number == 8);
        int dropped[3] = {0};
        for (int i = 1; i < o_max; i++)
        {
            if (o_list[i].k_idx)
                dropped[o_list[i].att] += o_list[i].number;
        }
        assert(dropped[0] == 3 && dropped[1] == 5 && dropped[2] == 0);
        assert(p_ptr->energy_use == 100 && p_ptr->previous_action[0] == ACTION_MISC);
        puts("Confirm replacement across two stacks drops exactly eight arrows and spends one turn PASS.");
    }
}

static void check_floor_failure(bool limited_pool)
{
    reset_map(5);
    player_quiver_reset_store();
    p_ptr->stat_use[A_STR] = 3;
    p_ptr->energy_use = 0;
    p_ptr->previous_action[0] = ACTION_NOTHING;
    selections = 0;
    cancel_second = false;

    object_type first = arrows(0, 3);
    object_type second = arrows(1, 45);
    object_type incoming = arrows(2, 8);
    assert(player_quiver_absorb_arrow(&first) == 3);
    assert(player_quiver_absorb_arrow(&second) == 45);
    assert(player_quiver_select_arrow(QUIVER_INDEX + 1));

    int y = p_ptr->py, x = p_ptr->px;
    if (!limited_pool)
    {
        for (int row = 0; row < p_ptr->cur_map_hgt; row++)
            for (int col = 0; col < p_ptr->cur_map_wid; col++)
                cave_feat[row][col] = FEAT_WALL_EXTRA;
        cave_feat[y][x] = cave_feat[y][x+1] = cave_feat[y+1][x] = FEAT_FLOOR;
        /* The 5-arrow drop has a free tile. The 3-arrow drop can only
         * partly merge with the 47 arrows in the other available tile. */
        object_type almost_full = arrows(0, 47);
        assert(floor_carry(y, x+1, &almost_full) == 1);
    }
    int item = floor_carry(y, x, &incoming);
    assert(item > 0);
    int saved_max = z_info->o_max;
    if (limited_pool)
        z_info->o_max = o_max + 1;
    int before_cnt = o_cnt, before_max = o_max;
    object_type before[3];
    memcpy(before, &o_list[1], sizeof(before));

    bool moved = floor_context_perform_action(-item, FLOOR_CONTEXT_ACTION_QUIVER);
    z_info->o_max = saved_max;
    assert(!moved && selections == 2);
    assert(player_quiver_store_entry_count() == 2);
    assert(player_quiver_selected_arrow_slot() == QUIVER_INDEX + 1);
    assert(player_quiver_store_entry_at(0)->number == 3);
    assert(player_quiver_store_entry_at(1)->number == 45);
    assert(o_cnt == before_cnt && o_max == before_max);
    assert(!memcmp(before, &o_list[1], sizeof(before)));
    assert(cave_o_idx[y][x] == item && !cave_o_idx[y+1][x]);
    assert(p_ptr->energy_use == 0 && p_ptr->previous_action[0] == ACTION_NOTHING);
    puts(limited_pool ? "Insufficient object slots leave both planned drops uncommitted PASS."
        : "A refused partial floor merge leaves both planned drops uncommitted PASS.");
    if (!limited_pool)
    {
        /* A second landing beyond the local search still permits the swap. */
        cave_feat[y][x+15] = FEAT_FLOOR;
        selections = 0;
        assert(floor_context_perform_action(-item, FLOOR_CONTEXT_ACTION_QUIVER));
        assert(o_list[cave_o_idx[y][x+15]].number == 3);
        assert(o_list[cave_o_idx[y+1][x]].number == 5);
        assert(o_list[cave_o_idx[y][x+1]].number == 47);
        assert(player_quiver_arrow_count() == QUIVER_ARROW_CAPACITY);
    }
}

static void check_merges_at_capacity(void)
{
    reset_map(5);
    player_quiver_reset_store();
    p_ptr->stat_use[A_STR] = 3;
    p_ptr->energy_use = 0;
    selections = 0;
    cancel_second = false;

    object_type first = arrows(0, 3);
    object_type second = arrows(1, 45);
    object_type incoming = arrows(0, 8);
    assert(player_quiver_absorb_arrow(&first) == 3);
    assert(player_quiver_absorb_arrow(&second) == 45);
    assert(player_quiver_select_arrow(QUIVER_INDEX + 1));

    int y = p_ptr->py, x = p_ptr->px;
    object_type nearby_first = arrows(0, 5);
    object_type nearby_second = arrows(1, 5);
    int item = floor_carry(y, x, &incoming);
    int floor_first = floor_carry(y, x+1, &nearby_first);
    int floor_second = floor_carry(y+1, x, &nearby_second);
    assert(item > 0 && floor_first > 0 && floor_second > 0);
    int saved_max = z_info->o_max;
    z_info->o_max = o_max;

    bool moved = floor_context_perform_action(-item, FLOOR_CONTEXT_ACTION_QUIVER);
    z_info->o_max = saved_max;
    assert(moved && selections == 2 && p_ptr->energy_use == 100);
    assert(!cave_o_idx[y][x] && o_cnt == 2);
    assert(o_list[floor_first].number == 8 && o_list[floor_second].number == 10);
    assert(player_quiver_arrow_count() == QUIVER_ARROW_CAPACITY);
    assert(player_quiver_selected_arrow_slot() == QUIVER_INDEX);
    assert(player_quiver_store_entry_at(0)->att == 1);
    assert(player_quiver_store_entry_at(0)->number == 40);
    assert(player_quiver_store_entry_at(1)->att == 0);
    assert(player_quiver_store_entry_at(1)->number == 8);
    puts("Full object pool allows nearby full merges without touching the incoming source PASS.");
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix, init = HARNESS.split("int main(int argc,char** argv)", 1)
    source = OUT / "check.c"
    source.write_text(prefix + CHECKS + "int main(int argc,char** argv)" +
                      init.split("    check_templates();", 1)[0] +
                      "    assert(init_flavor_info()==0); flavor_init();\n"
                      "    check_arrow_drop_near_full_stack();\n"
                      "    check_replacement(true); check_replacement(false);\n"
                      "    check_floor_failure(false); check_floor_failure(true);\n"
                      "    check_merges_at_capacity();\n"
                      "    SDL_Quit(); return 0;\n}\n", encoding="utf-8")
    sources = ("src/cmd/item/cmd-item-core.c", "src/object/object-place.c")
    excluded = ("/src/main.c.obj", *(f"/{path}.obj" for path in sources))
    build_target = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((build_target / "objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join(f'"{obj}"' for obj in objects
                                  if not obj.endswith(excluded)),
                        encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / dep) for dep in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"],
    ])
    exe = OUT / "check.exe"
    subprocess.run([
        "C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
        *(str(ROOT / path) for path in sources),
        "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
        "-Wl,--wrap=get_quantity_action", "-Wl,--wrap=object_choice_overlay",
        "-o", str(exe),
    ], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="fixture-", dir=OUT) as state:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), state],
                       cwd=state, env=env, check=True, timeout=15)


if __name__ == "__main__":
    main()

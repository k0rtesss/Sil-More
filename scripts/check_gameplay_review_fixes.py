#!/usr/bin/env python3
"""Exercise carried-store lore, Revealing, oath flags, Fletchery and song swaps.

Links the configured Windows engine objects, recompiling the changed production
units. All generated files/template caches stay under ignored scripts/output.
The Fletchery picker alone is wrapped; selection and completion use real code.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile

import check_new_monsters as engine

ROOT = engine.ROOT
BUILD = engine.BUILD
OUT = ROOT / "scripts/output/gameplay-review-fixes"

CHECKS = r'''
#include "player/player-upkeep-internal.h"
#include "player/player-song-internal.h"
#include "fletchery-under-test.c"

static int selected_item;
bool __wrap_open_inventory_item_select_menu(int mode, cptr reason,
    cptr no_item, int* selected)
{
    (void)mode; (void)reason; (void)no_item;
    *selected = selected_item;
    return true;
}

static void reset_carried(void)
{
    reset_map(10);
    player_carried_extra_reset_store();
    player_quiver_reset_store();
    memset(inventory, 0, INVEN_TOTAL * sizeof(*inventory));
    clear_fletchery_source_snapshot();
    p_ptr->leaving = false;
    p_ptr->fletching = 0;
    p_ptr->song1 = p_ptr->song2 = SNG_NOTHING;
    p_ptr->skill_stat_mod[S_PER] = p_ptr->skill_stat_mod[S_SMT] = 0;
    memset(p_ptr->have_ability, 0, sizeof(p_ptr->have_ability));
    memset(p_ptr->active_ability, 0, sizeof(p_ptr->active_ability));
}

static void check_extra_lore_and_oath(void)
{
    reset_carried();
    object_type item;
    object_prep(&item, lookup_kind(a_info[119].tval, a_info[119].sval));
    item.name1 = 119;
    apply_magic(&item, -1, true, true, true, true);
    object_known(&item);
    a_info[119].found_num = 0;
    p_ptr->ident_exp = p_ptr->exp = p_ptr->new_exp = 0;
    assert(player_carried_extra_load(&item));
    update_lore(0);
    assert(a_info[119].found_num == 1);
    assert(p_ptr->ident_exp == 100 && p_ptr->exp == 100);
    update_lore(0);
    assert(p_ptr->ident_exp == 100 && p_ptr->exp == 100);
    assert(player_has_inventory_flag3(TR3_OATH_NEGATE));
    player_carried_extra_reset_store();
    assert(!player_has_inventory_flag3(TR3_OATH_NEGATE));

    object_prep(&item, lookup_kind(TV_ARROW, SV_NORMAL_ARROW));
    item.name2 = 169;
    object_known(&item);
    e_info[169].aware = false;
    assert(player_carried_extra_load(&item));
    update_lore(0);
    assert(e_info[169].aware && p_ptr->ident_exp == 175);
    update_lore(0);
    assert(p_ptr->ident_exp == 175);
    puts("Expandable-store artefact XP and ego awareness: exactly once; carried oath negation: PASS.");
}

static void prepare_identification_skill(int skill)
{
    p_ptr->skill_use[S_PER] = skill + 3;
    p_ptr->skill_use[S_SMT] = p_ptr->stat_use[A_GRA] = 0;
    e_info[169].aware = true;
}

static void check_revealing(void)
{
    for (int quiver = 0; quiver < 2; quiver++)
    {
        reset_carried();
        object_type item;
        object_prep(&item, lookup_kind(TV_ARROW, SV_NORMAL_ARROW));
        item.name2 = 169;
        if (quiver) assert(player_quiver_absorb_arrow(&item) == 1);
        else assert(player_carried_extra_load(&item));
        object_type* carried = quiver ? player_quiver_store_entry_at(0)
            : player_carried_extra_entry_at(0);
        int difficulty = object_smithing_difficulty(carried);

        prepare_identification_skill(difficulty - 1);
        assert(!player_try_identify_smithing_object_on_examine(carried, false));
        p_ptr->song1 = SNG_REVEALING;
        assert(player_try_identify_smithing_object_on_examine(carried, false));
        assert(object_known_p(carried));

        carried->ident &= ~IDENT_KNOWN;
        sing_song_of_revealing(10);
        assert(object_known_p(carried));
    }
    puts("Revealing identifies expandable/Quiver objects and grants +1d5 at the exact threshold on examination: PASS.");
}

static object_type plain_arrows(int count, int attack)
{
    object_type item;
    object_prep(&item, lookup_kind(TV_ARROW, SV_NORMAL_ARROW));
    item.number = count;
    item.att = attack;
    return item;
}

static int quiver_count_with_attack(int attack)
{
    int count = 0;
    for (int i = 0; i < player_quiver_store_entry_count(); i++)
    {
        const object_type* arrow = player_quiver_store_entry_at(i);
        if (arrow->att == attack) count += arrow->number;
    }
    return count;
}

static void start_fletching(int handle, int expected)
{
    selected_item = handle;
    p_ptr->active_ability[S_ARC][ARC_FLETCHERY] = true;
    do_cmd_fletchery();
    assert(p_ptr->fletching == expected && p_ptr->energy_use == 100);
}

static void check_quiver_fletchery(void)
{
    for (int partial = 0; partial < 2; partial++)
    {
        reset_carried();
        object_type other = plain_arrows(4, 1);
        object_type source = plain_arrows(41, 0);
        object_type last = plain_arrows(3, 2);
        assert(player_quiver_absorb_arrow(&other) == 4);
        assert(player_quiver_absorb_arrow(&source) == 41);
        assert(player_quiver_absorb_arrow(&last) == 3);
        start_fletching(QUIVER_INDEX + 1, 41);
        if (partial)
        {
            p_ptr->fletching = 3;
            disturb(0, 0);
            assert(p_ptr->fletching == 0);
        }
        else finish_fletching(0);
        assert(player_quiver_arrow_count() == QUIVER_ARROW_CAPACITY);
        assert(quiver_count_with_attack(3) == (partial ? 38 : 41));
        assert(quiver_count_with_attack(0) == (partial ? 3 : 0));
        assert(quiver_count_with_attack(1) == 4 && quiver_count_with_attack(2) == 3);
        assert(player_pack_entry_count() == 0 && !cave_o_idx[p_ptr->py][p_ptr->px]);
    }

    /* Reordering/splitting during upkeep must not strand the source handle. */
    reset_carried();
    object_type source = plain_arrows(8, 0);
    assert(player_quiver_absorb_arrow(&source) == 8);
    start_fletching(QUIVER_INDEX, 8);
    player_quiver_remove_arrows(QUIVER_INDEX, 8);
    object_type other = plain_arrows(2, 1);
    assert(player_quiver_absorb_arrow(&other) == 2);
    source = plain_arrows(4, 0);
    source.discount = 10;
    assert(player_quiver_absorb_arrow(&source) == 4);
    source = plain_arrows(4, 0);
    assert(player_quiver_absorb_arrow(&source) == 4);
    source = plain_arrows(4, 0);
    source.discount = 20;
    assert(player_quiver_absorb_arrow(&source) == 4);
    /* Matching sources split across distinct store stacks. */
    player_quiver_store_entry_at(1)->discount = 0;
    player_quiver_store_entry_at(3)->discount = 0;
    finish_fletching(0);
    assert(player_quiver_arrow_count() == 14);
    assert(quiver_count_with_attack(3) == 8 && quiver_count_with_attack(0) == 4);
    assert(quiver_count_with_attack(1) == 2);

    /* Fresh/legacy state has no transient snapshot; handle fallback is safe. */
    reset_carried();
    source = plain_arrows(5, 0);
    assert(player_quiver_absorb_arrow(&source) == 5);
    p_ptr->fletch_item = QUIVER_INDEX;
    finish_fletching(2);
    assert(player_quiver_arrow_count() == 5);
    assert(quiver_count_with_attack(3) == 3 && quiver_count_with_attack(0) == 2);
    reset_carried();
    inventory[INVEN_QUIVER1] = plain_arrows(5, 0);
    inventory[INVEN_QUIVER1].pickup_slot = INVEN_QUIVER1;
    start_fletching(INVEN_QUIVER1, 5);
    finish_fletching(0);
    assert(player_quiver_arrow_count() == 5 && quiver_count_with_attack(3) == 5);
    assert(!inventory[INVEN_QUIVER1].k_idx);

    for (int extra = 0; extra < 2; extra++)
    {
        reset_carried();
        source = plain_arrows(48, 0);
        inventory[0] = plain_arrows(48, 1);
        inventory[1] = plain_arrows(3, 2);
        if (extra) assert(player_carried_extra_load(&source));
        else object_copy(&inventory[2], &source);
        start_fletching(extra ? CARRIED_EXTRA_INDEX : 2, 48);
        finish_fletching(0);
        int total = 0;
        int by_attack[4] = {0};
        for (int i = 0; i < player_pack_entry_count(); i++)
        {
            object_type* arrow = player_pack_entry_at(i);
            assert(arrow->att >= 1 && arrow->att <= 3);
            total += arrow->number;
            by_attack[arrow->att] += arrow->number;
        }
        assert(total == 99 && player_quiver_arrow_count() == 0);
        assert(by_attack[3] == 48 && by_attack[1] == 48 && by_attack[2] == 3);
        assert(!cave_o_idx[p_ptr->py][p_ptr->px]);
    }
    puts("Fletchery: full/partial Quiver results, shifted/split handles, unchanged totals and destination, legacy/fallback, physical/expandable Pack: PASS.");
}

static void check_theme_exchange(void)
{
    reset_carried();
    p_ptr->active_ability[S_SNG][SNG_WOVEN_THEMES] = true;
    for (int song = SNG_CONTEST; song <= SNG_LAMENT; song++)
    {
        p_ptr->song1 = song;
        p_ptr->song2 = SNG_STAYING;
        p_ptr->song_target_idx = 1;
        p_ptr->song_target_song = song;
        p_ptr->energy_use = 0;
        change_song(SNG_EXCHANGE_THEMES);
        assert(p_ptr->song1 == song && p_ptr->song2 == SNG_STAYING);
        assert(p_ptr->song_target_idx == 1 && p_ptr->song_target_song == song);
        assert(p_ptr->energy_use == 0);
    }
    p_ptr->song1 = SNG_ELBERETH;
    p_ptr->song2 = SNG_TREES;
    change_song(SNG_EXCHANGE_THEMES);
    assert(p_ptr->song1 == SNG_TREES && p_ptr->song2 == SNG_ELBERETH);
    assert(p_ptr->energy_use == 100);
    puts("Contest/Lament remain primary without spending time; ordinary theme exchange works: PASS.");
}
'''


def main():
    global BUILD
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--portable", action="store_true")
    args = parser.parse_args()
    BUILD = ROOT / ("build-portable" if args.portable else "build-standard")
    OUT.mkdir(parents=True, exist_ok=True)
    # externs.h is already included by the engine fixture and has unguarded
    # declarations. Keep the real Fletchery implementation in this same unit
    # to inspect its transient snapshot without adding a production test API.
    (OUT / "fletchery-under-test.c").write_text(
        (ROOT / "src/cmd/item/cmd-fletchery.c").read_text(encoding="utf-8")
        .replace('#include "externs.h"', ""), encoding="utf-8")
    prefix, main_body = engine.HARNESS.split("int main(int argc,char** argv)", 1)
    setup = main_body.split("    check_templates();", 1)[0]
    source = OUT / "check.c"
    source.write_text(prefix + CHECKS + "int main(int argc,char** argv)" + setup
        + "    check_extra_lore_and_oath();\n    check_revealing();\n"
        + "    check_quiver_fletchery();\n    check_theme_exchange();\n"
        + "    SDL_Quit(); return 0;\n}\n", encoding="utf-8")
    changed = ["src/player/player-lore.c", "src/player/player-song-revealing.c",
        "src/player/player-light.c", "src/player/player-songs.c"]
    excluded = ["src/main.c", "src/cmd/item/cmd-fletchery.c", *changed]
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects
        if not any(obj.endswith("/" + path + ".obj") for path in excluded)),
        encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    defines = ["-DUSE_SDL"] + (["-DSIL_USE_LOCAL_DATA"] if args.portable else [])
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", *defines, "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
        *(str(ROOT / path) for path in changed), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp",
        "-Wl,--wrap=open_inventory_item_select_menu", "-o", str(exe)],
        cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="data-", dir=OUT) as data:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), data],
            cwd=data, env=env, check=True, timeout=45)


if __name__ == "__main__":
    main()

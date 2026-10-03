#!/usr/bin/env python3
"""Exercise new Insight light suppression and adjacent sight in production.

Requires a current build-incremental.ps1. Uses initialized engine objects and
temporary raw data; never reads or writes player saves or configuration.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/insight-lighting"

TESTS = r'''
#include "player/player-upkeep-internal.h"
#include "ui/command-reference.h"
#include "rng.h"

static void dark_map(void)
{
    forget_view();
    memset(p_ptr, 0, sizeof(*p_ptr));
    memset(inventory, 0, INVEN_TOTAL * sizeof(*inventory));
    reset_map(10);
    p_ptr->cur_map_hgt = p_ptr->cur_map_wid = 24;
    p_ptr->py = p_ptr->px = 10;
    p_ptr->chp = p_ptr->mhp = 100;
    p_ptr->food = PY_FOOD_FULL;
    p_ptr->insight_ruleset = INSIGHT_RULESET_REWORKED;
    p_ptr->song1 = p_ptr->song2 = SNG_NOTHING;
    character_dungeon = true;
    character_generated = true;
    playerturn = 100;
    for (int y=0; y<24; y++) for (int x=0; x<24; x++) {
        bool border = !y || !x || y==23 || x==23;
        cave_feat[y][x] = border ? FEAT_WALL_PERM : FEAT_FLOOR;
        cave_info[y][x] = border ? CAVE_WALL : 0;
        cave_light[y][x] = 0;
        cave_natural[y][x] = 0;
    }
    cave_m_idx[10][10] = -1;
}

static void equip_light(int sval)
{
    int k = lookup_kind(TV_LIGHT, sval);
    assert(k);
    object_prep(&inventory[INVEN_LITE], k);
    inventory[INVEN_LITE].number = 1;
    if (fuelable_light_p(&inventory[INVEN_LITE]))
        player_light_set_fuel(&inventory[INVEN_LITE], 1000);
}

static void test_sources(void)
{
    dark_map();
    equip_light(SV_LIGHT_LESSER_JEWEL);
    calc_torch();
    int jewel = p_ptr->cur_light;
    assert(jewel > 0);
    do_cmd_dim_light(); calc_torch();
    assert(p_ptr->light_dimmed && !p_ptr->cur_light);
    do_cmd_dim_light(); calc_torch();
    assert(!p_ptr->light_dimmed && p_ptr->cur_light == jewel);

    /* Slot suppression covers both permanent and fueled lights. */
    const int sources[] = {SV_LIGHT_TORCH, SV_LIGHT_LANTERN,
        SV_LIGHT_MALLORN, SV_LIGHT_FEANORIAN, SV_LIGHT_SILMARIL};
    for (unsigned i=0; i<N_ELEMENTS(sources); i++) {
        equip_light(sources[i]); calc_torch();
        assert(p_ptr->cur_light > 0);
        int fuel = player_light_fuel(&inventory[INVEN_LITE]);
        do_cmd_dim_light(); calc_torch();
        assert(p_ptr->cur_light == 0);
        assert(player_light_fuel(&inventory[INVEN_LITE]) == fuel);
        do_cmd_dim_light();
    }
    equip_light(SV_LIGHT_LESSER_JEWEL);
    /* A synthetic luminous helmet uses the real object_flags/calculation. */
    int helm = 0;
    for (int i=1; i<z_info->k_max; i++)
        if (k_info[i].tval==TV_HELM) { helm=i; break; }
    assert(helm);
    u32b saved_flags = k_info[helm].flags2;
    k_info[helm].flags2 |= TR2_LIGHT;
    object_prep(&inventory[INVEN_HEAD], helm);
    inventory[INVEN_HEAD].number = 1;
    do_cmd_dim_light(); calc_torch();
    assert(p_ptr->cur_light == 1); /* ordinary dim retains the helmet */
    p_ptr->active_ability[S_WIL][WIL_INNER_LIGHT] = true;
    p_ptr->song1 = SNG_TREES;
    p_ptr->skill_use[S_SNG] = 30;
    p_ptr->active_ability[S_SPC][SPC_OATH_LIGHT] = true;
    calc_torch(); assert(p_ptr->cur_light > 1);
    int others = p_ptr->cur_light;
    p_ptr->active_ability[S_STL][STL_VEIL_OF_SHADOWS] = true;
    calc_torch(); assert(p_ptr->cur_light == 0);
    /* Exercise the real emission calculation with all positive sources and
     * Inner Light intensity still active, including the radius-zero centre. */
    update_view();
    assert(cave_light[10][10] == 0);
    for (int y=9; y<=11; y++) for (int x=9; x<=11; x++) {
        assert(cave_light[y][x] == 0);
        assert(!(cave_info[y][x] & CAVE_GLOW));
    }
    do_cmd_dim_light(); calc_torch();
    assert(p_ptr->cur_light == others + jewel);
    update_view();
    assert(cave_light[10][10] > 0 && cave_light[10][11] > 0);
    k_info[helm].flags2 = saved_flags;

    /* Both old modes ignore even an artificially set toggle/ability byte. */
    for (int mode=INSIGHT_RULESET_CLASSIC; mode<=INSIGHT_RULESET_LEGACY; mode++) {
        /* Legacy song scoring can differ: compare within the same ruleset. */
        p_ptr->insight_ruleset = mode; p_ptr->light_dimmed = false;
        calc_torch(); int legacy_light=p_ptr->cur_light;
        p_ptr->light_dimmed = true;
        calc_torch(); assert(p_ptr->cur_light == legacy_light);
        int count = COMMAND_SECONDARY_KEYBIND_COUNT;
        do_cmd_dim_light();
        assert(p_ptr->light_dimmed && count==(int)N_ELEMENTS(command_secondary_keybinds)-1);
    }
    dark_map(); do_cmd_dim_light(); assert(!p_ptr->light_dimmed);
    p_ptr->active_ability[S_STL][STL_VEIL_OF_SHADOWS] = true;
    do_cmd_dim_light(); assert(p_ptr->light_dimmed); /* works without a lamp */
    puts("Light sources: ordinary slot-only dim, all personal Veil emission, restore, no lamp and legacy isolation PASS.");
}

static void test_weapon_glow(void)
{
    dark_map();
    equip_light(SV_LIGHT_LESSER_JEWEL);
    int sword=0, orc=0;
    for (int i=1; i<z_info->k_max; i++)
        if (k_info[i].tval==TV_SWORD) { sword=i; break; }
    for (int i=1; i<z_info->r_max; i++)
        if ((r_info[i].flags3 & RF3_ORC)
            && !(r_info[i].flags1 & (RF1_UNIQUE | RF1_SPECIAL_GEN)))
            { orc=i; break; }
    assert(sword && orc);
    u32b flags=k_info[sword].flags1;
    k_info[sword].flags1 |= TR1_SLAY_ORC;
    object_prep(&inventory[INVEN_WIELD], sword);
    inventory[INVEN_WIELD].number=1;
    assert(place_monster_one(10,11,orc,false,false,NULL));
    update_view();
    assert(weapon_glows(&inventory[INVEN_WIELD]));
    do_cmd_dim_light(); calc_torch();
    assert(p_ptr->cur_light > 0);
    p_ptr->active_ability[S_STL][STL_VEIL_OF_SHADOWS]=true;
    calc_torch(); assert(!p_ptr->cur_light);
    k_info[sword].flags1=flags;
    puts("Production enemy-sensitive weapon glow survives ordinary dim and is suppressed by Veil PASS.");
}

static void test_sight(void)
{
    dark_map(); calc_torch(); update_view();
    s16b baseline[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    memcpy(baseline, cave_light, sizeof(baseline));
    p_ptr->active_ability[S_STL][STL_VEIL_OF_SHADOWS] = true;
    p_ptr->stealth_mode = true;
    update_view();
    for (int y=9; y<=11; y++) for (int x=9; x<=11; x++) {
        if (y==10 && x==10) continue;
        assert(player_dark_adjacency(y,x));
        assert(cave_info[y][x] & CAVE_SEEN);
        assert(!(cave_info[y][x] & CAVE_GLOW));
    }
    assert(!memcmp(baseline, cave_light, sizeof(baseline)));
    assert(!(cave_info[10][12] & CAVE_SEEN));
    assert(!(cave_info[10][12] & CAVE_MARK));
    assert(!player_dark_adjacency(10,12));
    assert(!player_dark_adjacency(-1,10));
    p_ptr->stealth_mode=false; update_view();
    assert(!(cave_info[10][11] & CAVE_SEEN));
    p_ptr->stealth_mode=true; p_ptr->blind=1; update_view();
    assert(!(cave_info[10][11] & CAVE_SEEN));
    p_ptr->blind=0; p_ptr->cur_light=1;
    assert(!player_dark_adjacency(10,11));
    p_ptr->cur_light=0;
    p_ptr->insight_ruleset=INSIGHT_RULESET_LEGACY; update_view();
    assert(!(cave_info[10][11] & CAVE_SEEN));
    p_ptr->insight_ruleset=INSIGHT_RULESET_REWORKED;

    /* Terrain and monster light still contribute through the production view. */
    cave_info[10][12] |= CAVE_GLOW; update_view();
    assert(cave_light[10][12] > 0 && (cave_info[10][12] & CAVE_SEEN));
    cave_info[10][12] &= ~CAVE_GLOW;
    cave_feat[12][10]=FEAT_LAVA; update_view();
    assert(cave_light[12][10] > 0);
    cave_feat[12][10]=FEAT_FLOOR;

    assert(place_monster_one(10,11,41,false,false,NULL));
    int idx=cave_m_idx[10][11];
    monster_type* m=&mon_list[idx];
    monster_race* r=&r_info[m->r_idx];
    u32b flags=r->flags2;
    int original_light=r->light;
    r->flags2 &= ~(RF2_INVISIBLE | RF2_GLOW);
    r->light=0;
    update_view(); update_mon(idx,true); assert(m->ml);
    r->flags2 |= RF2_INVISIBLE;
    p_ptr->skill_use[S_PER]=-100;
    m->ml=false; update_mon_for_generation(idx); assert(!m->ml);
    p_ptr->see_inv=100; update_mon_for_generation(idx); assert(m->ml);
    p_ptr->blind=1; m->ml=false; update_mon_for_generation(idx); assert(!m->ml);
    p_ptr->blind=0; r->flags2=flags | RF2_GLOW; r->light=2;
    update_view(); assert(cave_light[10][12] > 0);
    r->flags2=flags;
    r->light=original_light;
    puts("Sight: eight adjacent cells, fog distance, light isolation, blindness, invisibility checks and ambient/monster light PASS.");
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    harness = (prefix + fixture_function("terminal_extra") + "\n"
               + fixture_function("reset_map") + "\n" + TESTS + "\n" + init
               + '    test_sources(); test_weapon_glow(); test_sight(); SDL_Quit(); return 0;\n}\n')
    source = OUT / "check.c"
    source.write_text(harness, encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    rsp = OUT / "objects.rsp"
    rsp.write_text("\n".join('"' + p + '"' for p in objects
                             if not p.endswith("/src/main.c.obj")), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
                    "@" + str(rsp), "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-o", str(exe)],
                   cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="data-", dir=OUT) as data:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), data],
                       cwd=data, env=env, check=True, timeout=45)


if __name__ == "__main__":
    main()

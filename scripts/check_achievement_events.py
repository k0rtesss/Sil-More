#!/usr/bin/env python3
"""Exercise achievement event delivery through production combat, songs and smithing.

Requires a current Windows build. Engine templates and data use temporary
directories; achievement callbacks are observation doubles, so no profile,
platform account, save file or interactive frontend is opened.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/achievement-events"

SMITH = r'''
#include "cmd/ui/cmd-ui-smithing.c"
void fixture_prepare_smithing(bool custom, bool reforge)
{
    object_prep(smith_o_ptr, lookup_kind(TV_SWORD, SV_SHORT_SWORD));
    smith_o_ptr->number = 1;
    smith_o_ptr->unused1 = reforge ? 2 : 0;
    memset(&smithing_cost, 0, sizeof(smithing_cost));
    smithing_cost.exp = reforge ? 0 : 25;
    if (custom) {
        memset(smith_a_ptr, 0, sizeof(*smith_a_ptr));
        SDL_strlcpy(smith_a_ptr->name, "Event Fixture", sizeof(smith_a_ptr->name));
        smith_a_ptr->tval = smith_o_ptr->tval;
        smith_a_ptr->sval = smith_o_ptr->sval;
        smith_o_ptr->name1 = smith_a_name;
    }
    p_ptr->smithing = p_ptr->smithing_leftover = 1;
}
'''

CHECKS = r'''
#include "achievements.h"
#include "spell/spell-projection-internal.h"
void fixture_prepare_smithing(bool custom, bool reforge);
static int kills, last_who, light_kills, smiths, custom_smiths, singing_turns;
static bool force_floor, fail_floor;
void __wrap_sil_achievements_on_monster_killed(int idx, int who)
{
    assert(idx > 0 && idx < mon_max);
    assert(mon_list[idx].r_idx); /* Victim must still exist at delivery. */
    ++kills; last_who = who;
}
void __wrap_sil_achievements_on_light_kill(void) { ++light_kills; }
void __wrap_sil_achievements_on_singing_turn(void)
{
    assert(p_ptr->song1 < SNG_MAX && p_ptr->song_duration > 0);
    assert(p_ptr->csp > 0 && p_ptr->active_ability[S_SNG][p_ptr->song1]);
    ++singing_turns;
}
void __wrap_sil_achievements_on_smithing_completed(bool custom)
{
    ++smiths;
    if (custom) {
        assert(p_ptr->self_made_arts > 0);
        assert(a_info[z_info->art_rand_max + p_ptr->self_made_arts - 1].name[0]);
        ++custom_smiths;
    }
}
int __real_inven_carry(object_type* object, bool combine);
int __wrap_inven_carry(object_type* object, bool combine)
{
    return force_floor ? -1 : __real_inven_carry(object, combine);
}
s16b __real_drop_near(object_type* object, int chance, int y, int x);
s16b __wrap_drop_near(object_type* object, int chance, int y, int x)
{
    return fail_floor ? 0 : __real_drop_near(object, chance, y, x);
}
cptr __wrap_get_sdl_config_path(void) { return "fixture-sdl.json"; }
void __wrap_pause_with_text(const char desc[][100], int row, int col,
    const char extra[][100], byte attr)
{ (void)desc; (void)row; (void)col; (void)extra; (void)attr; }

static monster_type* victim(int race)
{
    memset(p_ptr, 0, sizeof(*p_ptr));
    reset_map(20);
    p_ptr->py = 10; p_ptr->px = 11;
    p_ptr->chp = p_ptr->mhp = 100;
    p_ptr->playing = true;
    current_build_vault_exact_token = true;
    assert(place_monster_one(10, 10, race, false, true, NULL));
    current_build_vault_exact_token = false;
    monster_type* m = &mon_list[cave_m_idx[10][10]];
    m->hp = 10; m->maxhp = 100;
    m->ml = true;
    m->alertness = ALERTNESS_ALERT;
    Rand_state_init(4321);
    return m;
}

static void check_deaths(void)
{
    monster_type* m = victim(41);
    int idx = cave_m_idx[10][10];
    assert(!mon_take_hit(idx, 1, "", -1));
    assert(!kills && m->hp == 9);
    assert(mon_take_hit(idx, 9, "", -1));
    assert(kills == 1 && last_who == -1 && !mon_list[idx].r_idx);
    m = victim(41); idx = cave_m_idx[10][10];
    assert(mon_take_hit(idx, m->hp, "", 0));
    assert(kills == 2 && last_who == 0);
    m = victim(41); idx = cave_m_idx[10][10];
    assert(mon_take_hit(idx, m->hp, "", 2));
    assert(kills == 3 && last_who == 2);
    m = victim(R_IDX_MORGOTH); idx = cave_m_idx[10][10];
    assert(!mon_take_hit(idx, m->hp, "", -1));
    assert(kills == 3 && mon_list[idx].r_idx && p_ptr->morgoth_second_wind);
    assert(mon_take_hit(idx, m->hp, "", -1));
    assert(kills == 4 && !mon_list[idx].r_idx);
    puts("Confirmed deaths: one callback before deletion, source preserved, no Morgoth phase-transition kill PASS.");
}

static void check_light(void)
{
    int race = 0;
    for (int i = 1; i < z_info->r_max; ++i)
        if ((r_info[i].flags3 & (RF3_UNDEAD | RF3_HURT_LITE))
            == (RF3_UNDEAD | RF3_HURT_LITE)) { race = i; break; }
    assert(race);
    int before = kills;
    monster_type* m = victim(race);
    m->hp = 1; cave_light[10][10] = 20;
    cave_info[10][10] = CAVE_VIEW | CAVE_SEEN;
    (void)project_m(-1, 10, 10, 20, 20, 1000, GF_LIGHT, PROJECT_SILENT);
    assert(!cave_m_idx[10][10] && kills == before + 1 && light_kills == 1);
    m = victim(race); m->hp = 30000;
    cave_light[10][10] = 20; cave_info[10][10] = CAVE_VIEW | CAVE_SEEN;
    (void)project_m(-1, 10, 10, 20, 20, 1000, GF_LIGHT, PROJECT_SILENT);
    assert(m->hp > 0 && kills == before + 1 && light_kills == 1);
    m = victim(41); m->hp = 1;
    u32b saved = r_info[41].flags3;
    r_info[41].flags3 = RF3_HURT_LITE; /* Susceptible living victim. */
    cave_light[10][10] = 20; cave_info[10][10] = CAVE_VIEW | CAVE_SEEN;
    (void)project_m(-1, 10, 10, 20, 20, 1000, GF_LIGHT, PROJECT_SILENT);
    r_info[41].flags3 = saved;
    assert(!cave_m_idx[10][10] && kills == before + 2 && light_kills == 1);
    m = victim(race); m->hp = 1;
    (void)project_m(-1, 10, 10, 20, 20, 1000, GF_ACID, PROJECT_SILENT);
    assert(!cave_m_idx[10][10] && light_kills == 1);
    m = victim(race); m->hp = 1;
    cave_light[10][10] = 20; cave_info[10][10] = CAVE_VIEW | CAVE_SEEN;
    (void)project_m(0, 10, 10, 20, 20, 1000, GF_LIGHT, PROJECT_SILENT);
    assert(m->hp == 1 && light_kills == 1);
    puts("Real light projection: only a player light kill of an undead delivers the light event PASS.");
}

static void check_smithing(void)
{
    memset(p_ptr, 0, sizeof(*p_ptr));
    memset(inventory, 0, INVEN_TOTAL * sizeof(*inventory));
    reset_map(10); p_ptr->new_exp = 1000;
    fixture_prepare_smithing(false, false);
    assert(!smiths); /* Blueprint and accepted work are insufficient. */
    create_smithing_item();
    assert(smiths == 1 && !custom_smiths && p_ptr->new_exp == 975);
    assert(!smith_o_ptr->k_idx);
    fixture_prepare_smithing(true, false);
    create_smithing_item();
    assert(smiths == 2 && custom_smiths == 1 && p_ptr->new_exp == 950);
    fixture_prepare_smithing(false, true);
    create_smithing_item();
    assert(smiths == 3 && custom_smiths == 1 && p_ptr->new_exp == 950);
    fixture_prepare_smithing(false, false); force_floor = true;
    create_smithing_item(); force_floor = false;
    assert(smiths == 4 && custom_smiths == 1 && p_ptr->new_exp == 925);
    assert(cave_o_idx[p_ptr->py][p_ptr->px]);
    force_floor = fail_floor = true;
    fixture_prepare_smithing(false, false); create_smithing_item();
    fixture_prepare_smithing(true, false); create_smithing_item();
    fixture_prepare_smithing(false, true); create_smithing_item();
    force_floor = fail_floor = false;
    assert(smiths == 4 && custom_smiths == 1);
    puts("Smithing: completed ordinary/custom/reforge/floor delivery, costs and artefact registration; no event on failed delivery PASS.");
}

static void check_poison(void)
{
    monster_type* m = victim(41);
    u32b saved = r_info[41].flags3;
    r_info[41].flags3 &= ~RF3_RES_POIS;
    int before = kills;
    m->hp = 1;
    (void)project_m(-1, 10, 10, 10, 1, 0, GF_POIS, PROJECT_SILENT);
    assert(m->poisoned == 10 && m->player_poisoned == 10 && kills == before);
    assert(monster_poison_tick(cave_m_idx[10][10]));
    assert(kills == before + 1 && last_who == -1);
    m = victim(41); m->hp = 1;
    monster_poison_add(cave_m_idx[10][10], 10);
    assert(!m->player_poisoned);
    assert(monster_poison_tick(cave_m_idx[10][10]));
    assert(kills == before + 2 && last_who == 0);
    m = victim(41); m->hp = 1;
    mon_list[2].r_idx = 41; mon_list[2].ml = true;
    (void)project_m(2, 10, 10, 10, 1, 0, GF_POIS, PROJECT_SILENT);
    assert(m->poisoned == 10 && !m->player_poisoned);
    assert(monster_poison_tick(cave_m_idx[10][10]));
    assert(kills == before + 3 && last_who == 0);
    r_info[41].flags3 = saved;
    puts("Real poison projections/ticks: player source survives delay; terrain and monster poison remain nonplayer PASS.");
}

static void check_singing(void)
{
    memset(p_ptr, 0, sizeof(*p_ptr));
    reset_map(10);
    p_ptr->playing = true;
    p_ptr->energy_use = 100;
    p_ptr->csp = p_ptr->msp = 20;
    p_ptr->song1 = p_ptr->song2 = SNG_NOTHING;
    for (int turn = 0; turn < 3; ++turn) sing();
    assert(!singing_turns && !p_ptr->song_duration && p_ptr->csp == 20);

    /* The first song has index zero; it is a real sustained theme. */
    assert(SNG_ELBERETH == 0);
    p_ptr->song1 = SNG_ELBERETH;
    p_ptr->active_ability[S_SNG][SNG_ELBERETH] = true;
    sing();
    assert(singing_turns == 1 && p_ptr->song_duration == 1);
    assert(p_ptr->csp < 20 && p_ptr->song_elbereth_effect >= 3);

    p_ptr->csp = 0;
    sing();
    assert(singing_turns == 1 && p_ptr->song1 == SNG_NOTHING);

    p_ptr->csp = 20;
    p_ptr->song1 = SNG_ELBERETH;
    p_ptr->active_ability[S_SNG][SNG_ELBERETH] = false;
    sing();
    assert(singing_turns == 1 && p_ptr->song1 == SNG_NOTHING);

    p_ptr->song1 = SNG_ELBERETH;
    p_ptr->song2 = SNG_SILENCE;
    p_ptr->active_ability[S_SNG][SNG_ELBERETH] = true;
    p_ptr->active_ability[S_SNG][SNG_SILENCE] = true;
    p_ptr->active_ability[S_SNG][SNG_WOVEN_THEMES] = true;
    sing();
    assert(singing_turns == 2 && p_ptr->song_duration == 1);
    assert(p_ptr->song1 == SNG_ELBERETH && p_ptr->song2 == SNG_SILENCE);
    sing();
    assert(singing_turns == 3 && p_ptr->song_duration == 2);

    /* Losing the minor theme leaves exactly one main-theme tick. */
    p_ptr->active_ability[S_SNG][SNG_WOVEN_THEMES] = false;
    sing();
    assert(singing_turns == 4 && p_ptr->song_duration == 3);
    assert(p_ptr->song2 == SNG_NOTHING);
    puts("Real sing(): silence/no Voice/unavailable main theme excluded; zero-index Elbereth and Woven Themes count once per sustained turn PASS.");
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    prefix += fixture_function("terminal_extra") + "\n" + fixture_function("reset_map")
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    source = OUT / "check.c"
    source.write_text(prefix + CHECKS + init + "    check_deaths(); check_light(); check_poison(); check_singing(); check_smithing();\n"
                      "    SDL_Quit(); return 0;\n}\n", encoding="utf-8")
    smith = OUT / "smith.c"
    smith.write_text(SMITH, encoding="utf-8")
    cmake_dir = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake_dir / "objects1.rsp").read_text())
    units = ("world/monster-death", "spell/spell-projection-effects", "player/player-songs")
    excluded = ("/src/main.c.obj", "/src/cmd/ui/cmd-ui-smithing.c.obj",
                *(f"/src/{unit}.c.obj" for unit in units))
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects
                                   if not obj.endswith(excluded)), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    wraps = ("sil_achievements_on_monster_killed", "sil_achievements_on_light_kill",
             "sil_achievements_on_singing_turn",
             "sil_achievements_on_smithing_completed", "inven_carry", "drop_near",
             "get_sdl_config_path", "pause_with_text")
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), str(smith),
                    *(str(ROOT / "src" / (unit + ".c")) for unit in units),
                    "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
                    *("-Wl,--wrap=" + name for name in wraps), "-o", str(exe)],
                   cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="data-", dir=OUT) as data:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), data], cwd=data,
                       env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()

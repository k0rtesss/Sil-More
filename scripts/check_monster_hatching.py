#!/usr/bin/env python3
"""Exercise Brood spider hatching and its action fallback with the real engine."""
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function
from check_new_monsters import BUILD, ROOT


CHECKS = r'''
#include "melee/melee-attack.h"
#include "melee/melee-process.h"

enum { BROOD_SPIDER = 44, HATCH_SPIDER = 96 + 16 };

static monster_type* brood_fixture(bool open_neighbour, bool capacity_blocked)
{
    monster_type* m = combat_fixture(BROOD_SPIDER, -100);
    view_n = 0;
    for (int y = 0; y < p_ptr->cur_map_hgt; y++)
        for (int x = 0; x < p_ptr->cur_map_wid; x++)
            if (!in_bounds_fully(y, x))
            {
                cave_feat[y][x] = FEAT_WALL_PERM;
                cave_info[y][x] = CAVE_WALL;
            }
    p_ptr->song1 = p_ptr->song2 = SNG_NOTHING;
    p_ptr->skill_base[S_EVN] = -100;
    cave_m_idx[p_ptr->py][p_ptr->px] = -1;
    for (int d = 0; d < 8; d++)
    {
        int y = m->fy + ddy_ddd[d], x = m->fx + ddx_ddd[d];
        if (open_neighbour && y == 9 && x == 10)
            continue;
        if (y != p_ptr->py || x != p_ptr->px)
            cave_set_feat(y, x, FEAT_WALL_EXTRA);
    }
    if (capacity_blocked)
        r_info[R_IDX_SPIDER_HATCHLING].max_num = 0;
    m->energy = 100;
    m->mana = MON_MANA_MAX;
    m->mflag |= MFLAG_ALWAYS_CAST;
    m->previous_action[0] = ACTION_MISC;
    l_list[BROOD_SPIDER].flags4 &= ~RF4_HATCH_SPIDER;
    l_list[BROOD_SPIDER].ranged = 0;
    assert((r_info[BROOD_SPIDER].flags4 & RF4_HATCH_SPIDER) != 0);
    character_generated = true;
    return m;
}

static int hatch_messages(void)
{
    int count = 0;
    for (int i = 0; i < message_num(); i++)
        if (strstr(message_str(i), "back hatches.")) count++;
    return count;
}

static void check_failed_cast(bool capacity_blocked)
{
    monster_type* m = brood_fixture(capacity_blocked, capacity_blocked);
    int old_messages = hatch_messages();
    bool cast = make_attack_ranged(m, HATCH_SPIDER);
    printf("Failed hatch (%s): cast=%d energy=%d mana=%d hatchlings=%d messages=%d.\n",
        capacity_blocked ? "capacity" : "blocked", cast, m->energy, m->mana,
        r_info[R_IDX_SPIDER_HATCHLING].cur_num, hatch_messages() - old_messages);
    assert(!cast && m->energy == 100 && m->mana == MON_MANA_MAX);
    assert(m->previous_action[0] == ACTION_MISC && (m->mflag & MFLAG_ALWAYS_CAST));
    assert(mon_cnt == 1 && !r_info[R_IDX_SPIDER_HATCHLING].cur_num);
    assert(hatch_messages() == old_messages);
    assert(!(l_list[BROOD_SPIDER].flags4 & RF4_HATCH_SPIDER));
}

static void check_ordinary_fallback(void)
{
    monster_type* m = brood_fixture(false, false);
    int old_messages = hatch_messages();
    update_flow(p_ptr->py, p_ptr->px, FLOW_PLAYER_NOISE);
    process_monsters(100);
    assert(m->energy == 0 && m->mana == MON_MANA_MAX && combat_number > 0);
    assert(mon_cnt == 1 && hatch_messages() == old_messages);
    assert(p_ptr->poisoned > 0);
    puts("Blocked hatch falls through to the Brood spider's ordinary bite: PASS.");

    m = brood_fixture(true, true);
    old_messages = hatch_messages();
    cave_m_idx[p_ptr->py][p_ptr->px] = 0;
    cave_set_feat(p_ptr->py, p_ptr->px, FEAT_WALL_EXTRA);
    p_ptr->py = 8;
    p_ptr->px = 10;
    cave_m_idx[p_ptr->py][p_ptr->px] = -1;
    m->cdis = 2;
    update_flow(p_ptr->py, p_ptr->px, FLOW_PLAYER_NOISE);
    process_monsters(100);
    assert(m->fy == 9 && m->fx == 10);
    assert(m->energy == 0 && m->mana == MON_MANA_MAX && mon_cnt == 1);
    assert(hatch_messages() == old_messages);
    puts("Capacity-blocked hatch falls through to ordinary movement: PASS.");
}

static void check_successful_cast(void)
{
    monster_type* m = brood_fixture(true, false);
    int old_messages = hatch_messages();
    m->mana = MON_MANA_COST - 1;
    assert(!make_attack_ranged(m, HATCH_SPIDER));
    assert(m->energy == 100 && m->mana == MON_MANA_COST - 1);
    assert(mon_cnt == 1 && hatch_messages() == old_messages);

    m->mana = MON_MANA_MAX;
    assert(make_attack_ranged(m, HATCH_SPIDER));
    int hatchling = cave_m_idx[9][10];
    assert(mon_cnt == 2 && hatchling > 0);
    assert(mon_list[hatchling].r_idx == R_IDX_SPIDER_HATCHLING);
    assert(m->energy == 150 && m->mana == MON_MANA_MAX - MON_MANA_COST);
    assert(m->previous_action[0] == ACTION_ARCHERY && !(m->mflag & MFLAG_ALWAYS_CAST));
    assert(hatch_messages() == old_messages + 1);
    assert((l_list[BROOD_SPIDER].flags4 & RF4_HATCH_SPIDER)
        && l_list[BROOD_SPIDER].ranged == 1);
    puts("One valid spawning square: one hatchling, one message and the usual half-turn cost: PASS.");
}

static void checks(void)
{
    check_failed_cast(false);
    check_failed_cast(true);
    check_ordinary_fallback();
    check_successful_cast();
}
'''


def main():
    out = ROOT / "scripts/output/monster-hatching"
    out.mkdir(parents=True, exist_ok=True)
    fixture = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    for name in ("terminal_extra", "reset_map", "combat_fixture"):
        fixture += fixture_function(name) + "\n"
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    fixture += CHECKS + init[:init.index("    check_templates();")]
    fixture += "    checks(); SDL_Quit(); return 0;\n}\n"
    source = out / "check.c"
    source.write_text(fixture, encoding="utf-8")
    replacements = ("src/melee/melee-attack-ranged.c", "src/player/player-song-monster.c")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    excluded = tuple(f"/{name}.obj" for name in (*replacements, "src/main.c"))
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
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="data-", dir=out) as data:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), data], cwd=data, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()

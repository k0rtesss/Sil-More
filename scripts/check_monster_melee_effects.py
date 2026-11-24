#!/usr/bin/env python3
"""Exercise monster Charge reactions and Shatter misses with the real engine."""
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function
from check_new_monsters import BUILD, ROOT


CHECKS = r'''
#include "melee/melee-attack.h"

static bool force_miss;
static int forced_margin, quakes;

int __real_hit_roll(int att, int evn, const monster_type* attacker,
    const monster_type* defender, bool display);
int __wrap_hit_roll(int att, int evn, const monster_type* attacker,
    const monster_type* defender, bool display)
{
    int result = __real_hit_roll(att, evn, attacker, defender, display);
    return force_miss ? forced_margin : result;
}

void __real_earthquake(int cy, int cx, int pit_y, int pit_x, int radius, int who);
void __wrap_earthquake(int cy, int cx, int pit_y, int pit_x, int radius, int who)
{
    quakes++;
    assert(cy == 10 && cx == 10 && who == cave_m_idx[10][10]);
    if (mon_list[who].r_idx == 162)
        assert(radius == 4 && pit_y == -1 && pit_x == -1);
    else
        assert(radius == 5 && pit_y == p_ptr->py && pit_x == p_ptr->px);
    __real_earthquake(cy, cx, pit_y, pit_x, radius, who);
}

static void check_monster_charge_history(void)
{
    for (int outside_action = 0; outside_action <= 1; outside_action++)
    {
        int observed[2];
        for (int moved = 0; moved <= 1; moved++)
        {
            monster_type* orc = combat_fixture(61, -100); /* Orc warrior */
            cave_m_idx[p_ptr->py][p_ptr->px] = -1;
            assert(r_info[orc->r_idx].flags2 & RF2_CHARGE);
            orc->stance = STANCE_CONFIDENT;
            orc->previous_action[0] = moved ? 6 : ACTION_MISC;
            orc->previous_action[1] = moved ? ACTION_MISC : 6;
            int attack = total_monster_attack(orc, r_info[orc->r_idx].blow[0].att);

            if (outside_action)
                attacks_of_opportunity(0, 0);
            else
            {
                monster_abilities_begin_action(orc);
                assert(make_attack_normal(orc));
            }

            assert(combat_number == 1 && combat_rolls[0][0].dam > 0);
            observed[moved] = combat_rolls[0][0].att - attack;
            assert(combat_rolls[0][0].ds == 6 + observed[moved]);
        }
        printf("Orc warrior %s: Charge accuracy after wait=%d, after approach=%d.\n",
            outside_action ? "reaction" : "scheduled attack", observed[0], observed[1]);
        assert(observed[0] == 0 && observed[1] == 3);
    }
}

static void check_shatter_visibility(int race)
{
    int outcomes[3];
    for (int state = 0; state < 3; state++)
    {
        for (int near_miss = 0; near_miss <= 1; near_miss++)
        {
            monster_type* attacker = combat_fixture(race, 100);
            cave_m_idx[p_ptr->py][p_ptr->px] = -1;
            assert(r_info[attacker->r_idx].blow[0].effect == RBE_SHATTER);
            attacker->ml = (state != 1);
            p_ptr->blind = (state == 1);
            p_ptr->confused = (state == 2);
            int hp = p_ptr->chp;

            quakes = 0;
            forced_margin = near_miss ? -1 : -10;
            force_miss = true;
            assert(make_attack_normal(attacker));
            force_miss = false;

            if (near_miss)
                outcomes[state] = hp - p_ptr->chp;
            else
                assert(!quakes && p_ptr->chp == hp);
            assert(quakes == near_miss);
        }
    }
    printf("Race %d near-miss earthquake damage: visible=%d, blind=%d, confused=%d.\n",
        race, outcomes[0], outcomes[1], outcomes[2]);
    assert(outcomes[1] == outcomes[0] && outcomes[2] == outcomes[0]);
    if (race == 162) assert(outcomes[0] > 0);
}

static void checks(void)
{
    check_monster_charge_history();
    check_shatter_visibility(162); /* Kemenrauko */
    check_shatter_visibility(R_IDX_MORGOTH);
    check_shatter_visibility(410); /* Ondotur */
    puts("Monster melee: Charge uses the last completed move and Shatter physics do not depend on narration: PASS.");
}
'''


def main():
    out = ROOT / "scripts/output/monster-melee-effects"
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
    excluded = ("/src/main.c.obj", "/src/melee/melee-attack.c.obj")
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
        str(ROOT / "src/melee/melee-attack.c"), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-Wl,--wrap=hit_roll",
        "-Wl,--wrap=earthquake", "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="data-", dir=out) as data:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), data], cwd=data, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()

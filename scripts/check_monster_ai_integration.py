#!/usr/bin/env python3
"""Exercise adaptive AI through the real engine, in isolated temporary data.

Build first. Reuses the initialized-template encounter fixture; no player save,
configuration, or interactive frontend is opened.
"""
import check_new_monsters as engine

CHECKS = r'''
#include "monster/monster-ai.h"
#include "monster/monster-senses.h"
#include "monster/monster-tactics.h"
#include "melee/melee-attack.h"
#include "spell/spell-projection-internal.h"
void process_player(void); /* Internal production scheduler entry point. */

static monster_type* adaptive_fixture(int race, int evasion)
{
    monster_type* m = combat_fixture(race, evasion);
    cave_m_idx[p_ptr->py][p_ptr->px] = -1;
    p_ptr->cur_light = 4;
    /* The fixture is open floor; establish its field of fire, which the
     * production projectable() fast path requires even for adjacent attacks. */
    for (int y=0; y<p_ptr->cur_map_hgt; ++y)
        for (int x=0; x<p_ptr->cur_map_wid; ++x)
            if (!y || !x || y==p_ptr->cur_map_hgt-1 || x==p_ptr->cur_map_wid-1) {
                cave_feat[y][x] = FEAT_WALL_PERM;
                cave_info[y][x] |= CAVE_WALL;
            }
            else if (distance(y,x,p_ptr->py,p_ptr->px) <= MAX_SIGHT)
                cave_info[y][x] |= CAVE_FIRE | CAVE_VIEW;
    return m;
}

static void check_adaptive_integration(void)
{
    monster_type* m = adaptive_fixture(403, 0);
    m->hp = m->maxhp = 20000;
    p_ptr->skill_use[S_MEL] = 100;
    p_ptr->mdd = p_ptr->mds = 1;
    playerturn = 100;
    py_attack_aux(10, 10, ATT_FLANKING);
    assert(monster_ai_confidence(m, MON_AI_FLANKING) == 3);
    playerturn++;
    py_attack_aux(10, 10, ATT_RIPOSTE);
    assert(monster_ai_confidence(m, MON_AI_RIPOSTE) == 3);
    puts("Production player attacks teach only actual Flanking/Riposte reactions PASS.");

    int dragon = 0;
    for (int i = 1; i < z_info->r_max; ++i)
        if ((r_info[i].flags3 & RF3_DRAGON)
            && (r_info[i].flags4 & RF4_BRTH_FIRE)) { dragon = i; break; }
    assert(dragon);
    m = adaptive_fixture(dragon, 0);
    p_ptr->resist_fire = 3;
    p_ptr->cur_light = 4;
    for (int i = 0; i < 3; ++i) {
        playerturn++;
        assert(project_p(cave_m_idx[10][10], 10, 11, 10, 10, 0, GF_FIRE));
    }
    assert(monster_ai_confidence(m, MON_AI_FIRE) == 3);
    int learned = monster_ranged_utility(m, 96 + 3);
    monster_ai_reset(m);
    assert(monster_ranged_utility(m, 96 + 3) > learned);
    p_ptr->resist_fire = 1;
    m->ml = false; /* The player seeing the monster is not a learning gate. */
    playerturn++;
    assert(project_p(cave_m_idx[10][10], 10, 11, 10, 10, 0, GF_FIRE));
    assert(monster_ai_confidence(m, MON_AI_FIRE) == -1);
    puts("Production fire projection teaches coarse witnessed resistance and changes breath utility PASS.");

    m = adaptive_fixture(403, 0);
    int hp = m->hp;
    m->alertness = ALERTNESS_MIN;
    m->poisoned = 10;
    assert(!monster_poison_tick(cave_m_idx[10][10]));
    assert(m->hp == hp - 2 && m->poisoned == 8);
    assert(m->alertness >= ALERTNESS_ALERT);
    m->hp = 1; m->poisoned = 10;
    int index = cave_m_idx[10][10];
    assert(monster_poison_tick(index));
    assert(!mon_list[index].r_idx && !cave_m_idx[10][10]);
    puts("Real mon_take_hit poison route wakes sleeping creatures and removes lethal victims PASS.");

    m = adaptive_fixture(411, 0);
    m->hp = m->maxhp = 100;
    m->poisoned = 6;
    cave_feat[10][10] = FEAT_POISON;
    m->smite_recovery = 1; m->skip_next_turn = true;
    m->energy = 100;
    process_monsters(100);
    assert(m->hp == 97 && m->poisoned == 9);
    assert(m->smite_recovery == 2 && m->fy == 10 && m->fx == 10);
    assert(!make_attack_reaction(m));
    m->energy = 100;
    process_monsters(100);
    assert(m->hp <= 94 && cave_feat[m->fy][m->fx] != FEAT_POISON);
    puts("Real scheduler: pool dose and poison tick run during Smite recovery, followed by escape PASS.");

    m = adaptive_fixture(407, 0);
    hp = m->hp;
    m->poisoned = 10; m->skip_next_turn = true;
    cave_feat[10][10] = FEAT_POISON;
    m->energy = 100;
    process_monsters(100);
    assert(m->hp == hp - 2 && m->poisoned == 8);
    puts("Flying poison-resistant creature avoids pool dose but retains previously applied poison PASS.");

    m = adaptive_fixture(403, 0);
    delete_monster_idx(cave_m_idx[m->fy][m->fx]);
    p_ptr->chp = p_ptr->mhp = 100;
    p_ptr->resist_pois = 1;
    p_ptr->restoring = true; /* Suppress between-turn presentation only. */
    p_ptr->entranced = 1; p_ptr->energy = 100;
    cave_feat[p_ptr->py][p_ptr->px] = FEAT_POISON;
    playerturn = 200;
    process_player();
    assert(playerturn == 201 && p_ptr->energy == 0);
    assert(p_ptr->chp == 98 && p_ptr->poisoned == 4);
    p_ptr->restoring = true; p_ptr->entranced = 1; p_ptr->energy = 100;
    cave_feat[p_ptr->py][p_ptr->px] = FEAT_FLOOR;
    process_player();
    assert(p_ptr->chp == 97 && p_ptr->poisoned == 3);
    puts("Real complete player actions apply pool contact before poison tick and suppress regeneration PASS.");
}
'''


def main():
    engine.OUT = engine.ROOT / "scripts/output/monster-ai-integration"
    engine.HARNESS = engine.HARNESS.replace(
        "int main(int argc,char** argv)", CHECKS + "\nint main(int argc,char** argv)")
    engine.HARNESS = engine.HARNESS.replace(
        "    check_combat();", "    check_combat();\n    check_adaptive_integration();")
    engine.main()


if __name__ == "__main__":
    main()

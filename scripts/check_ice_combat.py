#!/usr/bin/env python3
"""Check production ice combat/stats and movement using the isolated water map.

Build with build-incremental.ps1 first. No real saves or configuration are used.
"""
import check_water as water

TESTS = r'''
#include "player/player-upkeep-internal.h"
void land(void);
static void ice_combat_tests(void) {
    static player_race race;
    static character_profile profile;
    rp_ptr=&race; current_character_profile=&profile;
    c_info=calloc(1,sizeof(*c_info)); p_ptr->pcharacter=0;
    water_map(32,32,FEAT_FLOOR); cave_m_idx[10][10]=-1;
    character_generated=false; /* The isolated map has no FOV work buffers. */
    memset(p_ptr->active_ability,0,sizeof(p_ptr->active_ability));
    p_ptr->stun=p_ptr->entranced=0;
    p_ptr->skill_base[S_MEL]=20; p_ptr->skill_base[S_ARC]=20;
    p_ptr->skill_base[S_EVN]=20;
    calc_bonuses_for_preview();
    int melee=p_ptr->skill_use[S_MEL], archery=p_ptr->skill_use[S_ARC];
    int evasion=p_ptr->skill_use[S_EVN];
    monster_type* m=&mon_list[1]; memset(m,0,sizeof(*m));
    memset(&r_info[1],0,sizeof(r_info[1]));
    m->r_idx=1;m->fy=10;m->fx=12;m->ml=true;
    m->alertness=ALERTNESS_ALERT;r_info[1].evn=20;
    int attack=total_player_attack(m,melee);
    int bow_attack=total_player_attack(m,archery);
    int defence=total_player_evasion(m,false);
    cave_set_feat(10,10,FEAT_ICE);calc_bonuses_for_preview();
    assert(p_ptr->skill_use[S_MEL]==melee-2);
    assert(p_ptr->skill_use[S_ARC]==archery-2);
    assert(p_ptr->skill_use[S_EVN]==evasion-2);
    assert(total_player_attack(m,p_ptr->skill_use[S_MEL])==attack-2);
    assert(total_player_attack(m,p_ptr->skill_use[S_ARC])==bow_attack-2);
    assert(total_player_attack_ex(m,p_ptr->skill_use[S_MEL],false,false)==attack-2);
    assert(total_player_evasion(m,false)==defence-2);
    assert(total_player_evasion(m,true)==(defence-2)/2);
    calc_bonuses_for_preview();assert(p_ptr->skill_use[S_MEL]==melee-2);
    p_ptr->leaping=true;calc_bonuses_for_preview();
    assert(p_ptr->skill_use[S_MEL]==melee && p_ptr->skill_use[S_EVN]==evasion);
    land();assert(!p_ptr->leaping && p_ptr->skill_use[S_MEL]==melee-2);
    p_ptr->entranced=1;calc_bonuses_for_preview();
    assert(p_ptr->skill_use[S_EVN]==-5);
    p_ptr->entranced=0;calc_bonuses_for_preview();
    /* Production movement refreshes displayed skills immediately on exit/entry. */
    p_ptr->energy_use=100;move_player(6);
    assert(p_ptr->px==11 && p_ptr->energy_use==100);
    assert(p_ptr->skill_use[S_MEL]==melee && p_ptr->skill_use[S_EVN]==evasion);
    p_ptr->energy_use=100;move_player(4);
    assert(p_ptr->px==10 && p_ptr->energy_use==100);
    assert(p_ptr->skill_use[S_MEL]==melee-2 && p_ptr->skill_use[S_EVN]==evasion-2);
    /* Freeze/melt under the stationary player refresh the next roll at once. */
    character_dungeon=true;
    cave_set_feat(10,10,FEAT_WATER);
    assert(p_ptr->skill_use[S_MEL]==melee && p_ptr->skill_use[S_EVN]==evasion);
    cave_set_feat(10,10,FEAT_ICE);
    assert(p_ptr->skill_use[S_MEL]==melee-2 && p_ptr->skill_use[S_EVN]==evasion-2);
    character_dungeon=false;
    int monster_attack=total_monster_attack(m,20);
    int monster_evasion=total_monster_evasion(m,false);
    cave_set_feat(m->fy,m->fx,FEAT_ICE);
    assert(total_monster_attack(m,20)==monster_attack-2);
    assert(total_monster_evasion(m,false)==monster_evasion-2);
    assert(total_monster_evasion(m,true)==(monster_evasion-2)/2);
    r_info[1].flags2=RF2_FLYING;
    assert(total_monster_attack(m,20)==monster_attack);
    assert(total_monster_evasion(m,false)==monster_evasion);
    r_info[1].flags2=0;m->alertness=ALERTNESS_UNWARY-1;
    assert(total_monster_evasion(m,false)==-5);
    puts("Ice combat: displayed skills, melee/bows/throws, no double penalty, archery halving, flight/leaps/landing, movement refresh/energy, fixed helpless Evasion: PASS");
}
'''


def main():
    water.OUT = water.ROOT / "scripts/output/ice-combat-check"
    water.TESTS = water.TESTS.replace(
        "    vault_water_tests(); water_render_tests();",
        "    vault_water_tests(); water_render_tests(); ice_combat_tests();")
    water.TESTS = "static void ice_combat_tests(void);\n" + water.TESTS + TESTS
    water.main()


if __name__ == "__main__":
    main()

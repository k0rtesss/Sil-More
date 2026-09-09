"""Monster cases for the isolated lava integration harness.

Import TESTS after check_water.TESTS, then call lava_monster_tests() once that
harness has allocated the engine map/monster globals. Production entry,
passability and action scheduling functions are called directly.
"""

TESTS = r'''
#include "melee/melee-process.h"

static monster_type* lava_test_monster(u32b flags2, u32b flags3) {
    water_map(32,32,FEAT_FLOOR);
    p_ptr->py=p_ptr->px=25;
    p_ptr->leaving=false;
    cave_m_idx[25][25]=-1;
    monster_lava_end_action(1);
    memset(&mon_list[1],0,sizeof(mon_list[1]));
    memset(&r_info[1],0,sizeof(r_info[1]));
    memset(&l_list[1],0,sizeof(l_list[1]));
    monster_type* m=&mon_list[1];
    m->r_idx=1;m->fy=10;m->fx=10;m->hp=m->maxhp=200;
    m->alertness=ALERTNESS_ALERT;m->encountered=true;
    cave_m_idx[10][10]=1;mon_max=2;mon_cnt=1;
    r_info[1].name=1;r_info[1].cur_num=1;r_info[1].max_num=100;
    r_info[1].flags2=flags2;r_info[1].flags3=flags3;
    cave_set_feat(10,11,FEAT_LAVA);cave_set_feat(10,12,FEAT_LAVA);
    return m;
}

void lava_monster_tests(void) {
    bool bash=false;
    monster_type* m=lava_test_monster(0,0);
    assert(!cave_exist_mon(&r_info[1],10,11,false,false));
    assert(cave_passable_mon(m,10,11,&bash)==0);
    /* A player on lava can still be attacked from the bank. */
    cave_m_idx[10][11]=-1;
    assert(cave_passable_mon(m,10,11,&bash)==100);
    cave_m_idx[10][11]=0;
    monster_swap(10,10,10,11);
    assert(!m->r_idx && !cave_m_idx[10][11]);

    m=lava_test_monster(RF2_FLYING,0);
    assert(cave_exist_mon(&r_info[1],10,11,false,false));
    assert(cave_passable_mon(m,10,11,&bash)==100);
    monster_swap(10,10,10,11);
    assert(m->r_idx && m->hp==160 && m->fx==11);
    /* Starting and moving within lava is only one exposure per action. */
    assert(!monster_lava_begin_action(1));assert(m->hp==120);
    process_move(m,10,12,false);assert(m->hp==120 && m->fx==12);
    monster_lava_end_action(1);
    assert(!monster_lava_begin_action(1));assert(m->hp==80);
    monster_lava_end_action(1);

    for(int flyer=0;flyer<2;flyer++) {
        m=lava_test_monster(flyer ? RF2_FLYING : 0,RF3_RES_FIRE);
        assert(cave_exist_mon(&r_info[1],10,11,false,false));
        assert(cave_passable_mon(m,10,11,&bash)==100);
        monster_swap(10,10,10,11);
        assert(!monster_lava_begin_action(1));
        process_move(m,10,12,false);monster_lava_end_action(1);
        assert(m->r_idx && m->hp==200 && m->fx==12);
    }

    /* A skipped turn still burns; a ground creature cannot remain asleep
     * on lava restored from a save or created beneath it. */
    m=lava_test_monster(RF2_FLYING,0);
    monster_swap(10,10,10,11);m->skip_next_turn=true;m->energy=100;
    process_monsters(100);
    assert(m->r_idx && m->hp==120 && m->energy==0 && !m->skip_next_turn);
    m=lava_test_monster(0,0);
    cave_set_feat(10,10,FEAT_LAVA);
    m->alertness=ALERTNESS_UNWARY-1;m->energy=100;
    process_monsters(100);assert(!m->r_idx && !cave_m_idx[10][10]);
    puts("Lava monsters: path safety, forced entry, flight, resistance, per-action heat, skipped/sleeping turns: PASS");
}
'''

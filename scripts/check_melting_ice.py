#!/usr/bin/env python3
"""Exercise melting ice contact and shore rules in the production water harness."""
import check_water as water

TESTS = r'''
static void ice_force_roll(bool breaks) {
    for (u32b seed=1;seed<10000;seed++) {
        Rand_state_init(seed);
        bool result=one_in_(MELTING_ICE_BREAK_ONE_IN);
        if(result==breaks) { Rand_state_init(seed); return; }
    }
    assert(false);
}
static void melting_ice_tests(void) {
    static player_race race;static character_profile profile;
    rp_ptr=&race;current_character_profile=&profile;
    c_info=calloc(1,sizeof(*c_info));p_ptr->pcharacter=0;
    water_map(32,32,FEAT_FLOOR);cave_m_idx[10][10]=-1;
    character_generated=character_dungeon=false;
    p_ptr->leaving=p_ptr->is_dead=false;p_ptr->energy_use=100;
    cave_set_feat(10,10,FEAT_MELTING_ICE);
    assert(cave_floor_bold(10,10));
    assert(!cave_deep_water_allowed(10,10));
    for(int y=9;y<=11;y++)for(int x=9;x<=11;x++)cave_set_feat(y,x,FEAT_ICE);
    assert(cave_deep_water_allowed(10,10));
    for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++) {
        if(!dy&&!dx)continue;
        const int dry[]={FEAT_FLOOR,FEAT_WALL_EXTRA,FEAT_BRIDGE_WATER_H,FEAT_LESS};
        for(int i=0;i<4;i++) {
            cave_set_feat(10+dy,10+dx,dry[i]);
            assert(!cave_deep_water_allowed(10,10));
        }
        cave_set_feat(10+dy,10+dx,FEAT_WATER);
        assert(cave_deep_water_allowed(10,10));
    }
    assert(!cave_deep_water_allowed(0,0));
    int shallow=0,deep=0;
    for(int seed=1;seed<=1000;seed++) {
        cave_set_feat(10,10,FEAT_MELTING_ICE);Rand_state_init(seed);
        if(player_melting_ice_exposure()) {
            if(cave_feat[10][10]==FEAT_WATER)shallow++;
            else {assert(cave_feat[10][10]==FEAT_DEEP_WATER);deep++;}
        }
    }
    assert(shallow>50&&deep>50&&shallow+deep>150&&shallow+deep<250);
    cave_set_feat(9,9,FEAT_FLOOR);
    for(int seed=1;seed<=1000;seed++) {
        cave_set_feat(10,10,FEAT_MELTING_ICE);Rand_state_init(seed);
        (void)player_melting_ice_exposure();
        assert(cave_feat[10][10]!=FEAT_DEEP_WATER);
    }
    /* Entry plus action completion consumes one roll even on a failed break. */
    cave_set_feat(10,10,FEAT_MELTING_ICE);ice_force_roll(false);
    player_melting_ice_begin_action();assert(!player_melting_ice_exposure());
    u64b after=Rand_state_export();player_melting_ice_end_action();assert(Rand_state_export()==after);
    /* Cancelled commands and an airborne midpoint consume no roll. */
    p_ptr->energy_use=0;player_melting_ice_begin_action();after=Rand_state_export();
    player_melting_ice_end_action();assert(Rand_state_export()==after);
    p_ptr->energy_use=100;p_ptr->leaping=true;after=Rand_state_export();
    assert(!player_melting_ice_exposure());assert(Rand_state_export()==after);
    p_ptr->leaping=false;
    /* Cold makes weak ice safe; fire melts it subject to the same shore rule. */
    assert(cave_transform_elemental_terrain(10,10,GF_COLD));
    assert(cave_feat[10][10]==FEAT_ICE);after=Rand_state_export();
    assert(!player_melting_ice_exposure());assert(Rand_state_export()==after);
    cave_set_feat(10,10,FEAT_MELTING_ICE);
    assert(cave_transform_elemental_terrain(10,10,GF_FIRE));
    assert(cave_feat[10][10]==FEAT_WATER);
    /* Production movement breaks on entry, before water movement energy. */
    water_map(32,32,FEAT_FLOOR);cave_m_idx[10][10]=-1;
    p_ptr->energy_use=100;p_ptr->diseased=true;
    cave_set_feat(10,11,FEAT_MELTING_ICE);ice_force_roll(true);
    player_melting_ice_begin_action();move_player(6);
    assert(p_ptr->px==11&&cave_feat[10][11]==FEAT_WATER);
    assert(p_ptr->energy_use==150);
    player_melting_ice_end_action();
    /* Forced monster movement and stationary/sleeping turns use the same contact. */
    water_map(32,32,FEAT_FLOOR);cave_m_idx[10][10]=-1;mon_max=2;
    monster_type* m=&mon_list[1];memset(m,0,sizeof(*m));memset(&r_info[1],0,sizeof(r_info[1]));
    m->r_idx=1;m->fy=12;m->fx=12;m->hp=m->maxhp=10;m->alertness=ALERTNESS_UNWARY-1;
    cave_m_idx[12][12]=1;cave_set_feat(12,13,FEAT_MELTING_ICE);
    ice_force_roll(true);monster_swap(12,12,12,13);
    assert(cave_feat[12][13]==FEAT_WATER);
    cave_set_feat(12,13,FEAT_MELTING_ICE);ice_force_roll(false);
    monster_melting_ice_begin_action(1);after=Rand_state_export();
    monster_melting_ice_exposure(1);assert(Rand_state_export()==after);
    monster_melting_ice_end_action(1);
    ice_force_roll(true);monster_melting_ice_begin_action(1);
    assert(cave_feat[12][13]==FEAT_WATER);monster_melting_ice_end_action(1);
    cave_set_feat(12,13,FEAT_MELTING_ICE);r_info[1].flags2=RF2_FLYING;after=Rand_state_export();
    monster_melting_ice_begin_action(1);monster_melting_ice_end_action(1);
    assert(cave_feat[12][13]==FEAT_MELTING_ICE&&Rand_state_export()==after);
    puts("Melting ice: 20% rolls, shallow/deep results, diagonal shore/bridge clearance, action guards, flight/leaps, elements and production actor movement: PASS");
}
'''

def main():
    water.OUT = water.ROOT / "scripts/output/melting-ice-check"
    water.TESTS = water.TESTS.replace(
        "    vault_water_tests(); water_render_tests();",
        "    vault_water_tests(); water_render_tests(); melting_ice_tests();")
    water.TESTS = "static void melting_ice_tests(void);\n" + water.TESTS + TESTS
    water.main()

if __name__ == "__main__":
    main()

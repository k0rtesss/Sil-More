#!/usr/bin/env python3
"""Check submerged item discovery and spawning with production game objects.

Build first. Reuses the isolated SDL water harness; no saves/config are opened.
"""
import check_water as water

SUBMERGED_TESTS = r'''
#include "cave/cave-flood.h"

static void submerged_map(void) {
    water_map(32,32,FEAT_FLOOR);
    mon_max=1;
    for(int i=0;i<32;i++) {
        cave_set_feat(0,i,FEAT_WALL_PERM);cave_set_feat(31,i,FEAT_WALL_PERM);
        cave_set_feat(i,0,FEAT_WALL_PERM);cave_set_feat(i,31,FEAT_WALL_PERM);
    }
    memset(o_list,0,64*sizeof(*o_list));o_max=1;o_cnt=0;z_info->o_max=64;
    p_ptr->skill_use[S_PER]=0;p_ptr->cur_light=5;
    k_name="\0test item\0";k_info[1].name=1;k_info[1].tval=TV_GEM;
    if(!flavor_info)flavor_info=calloc(1,sizeof(*flavor_info));
    flavor_text="test";k_info[1].flavor=0;
    k_info[1].x_attr=TILE_FLAG|1;k_info[1].x_char=(char)(TILE_FLAG|10);
    k_info[1].d_attr=TERM_RED;k_info[1].d_char='*';
}
static object_type submerged_item(int tval) {
    object_type obj={0};obj.k_idx=1;obj.tval=tval;obj.number=1;return obj;
}
static void submerged_items_tests(void) {
    submerged_map();cave_m_idx[10][10]=-1;
    cave_set_feat(10,11,FEAT_DEEP_WATER);cave_set_feat(10,12,FEAT_DEEP_WATER);
    p_ptr->energy_use=100;move_player(6);
    assert(p_ptr->px==11&&p_ptr->energy_use==400);
    p_ptr->energy_use=100;move_player(6);
    assert(p_ptr->px==12&&p_ptr->energy_use==400);
    p_ptr->active_ability[S_EVN][EVN_LEAPING]=false;
    submerged_map();cave_m_idx[10][10]=-1;
    cave_set_feat(10,11,FEAT_BRIDGE_DEEP_WATER_H);
    p_ptr->energy_use=100;move_player(6);assert(p_ptr->px==11);
    /* A leap may land in deep water, paying the submerged movement cost. */
    submerged_map();cave_m_idx[10][10]=-1;cave_set_feat(10,11,FEAT_WATER);
    p_ptr->active_ability[S_EVN][EVN_LEAPING]=true;
    p_ptr->previous_action[1]=6;p_ptr->energy_use=100;move_player(6);
    assert(p_ptr->px==11&&p_ptr->leaping);
    cave_set_feat(10,12,FEAT_DEEP_WATER);continue_leap();
    assert(p_ptr->px==12&&!p_ptr->leaping&&p_ptr->energy_use==400);
    p_ptr->active_ability[S_EVN][EVN_LEAPING]=false;
    puts("Deep water movement: walking, leaping and bridge crossings are allowed with the submerged cost: PASS");
    const int liquids[]={FEAT_WATER,FEAT_DEEP_WATER};
    for(int i=0;i<2;i++) {
        submerged_map();cave_set_feat(10,13,liquids[i]);
        object_type obj=submerged_item(TV_GEM);
        int idx=floor_carry(10,13,&obj);assert(idx>0);
        object_type* item=&o_list[idx];
        assert(!item->marked&&!object_is_visible(item));
        note_spot(10,13);assert(!item->marked);
        p_ptr->skill_use[S_PER]=14;note_spot(10,13);assert(!item->marked);
        p_ptr->skill_use[S_PER]=15;note_spot(10,13);
        assert(item->marked&&object_is_visible(item));
        byte a,ta;char c,tc;map_info(10,13,&a,&c,&ta,&tc);
        assert(a==object_attr(item)&&c==object_char(item));
        p_ptr->skill_use[S_PER]=0;assert(!object_is_visible(item)&&item->marked);
        map_info(10,13,&a,&c,&ta,&tc);assert(a!=object_attr(item)||c!=object_char(item));
        p_ptr->px=13;p_ptr->skill_use[S_PER]=-5;assert(object_is_visible(item));
        p_ptr->blind=true;assert(!object_is_visible(item));p_ptr->blind=false;
        cave_info[10][13]&=~CAVE_SEEN;assert(!object_is_visible(item));
        cave_info[10][13]|=CAVE_SEEN;
        cave_set_feat(10,13,FEAT_BRIDGE_WATER_H);p_ptr->px=10;
        assert(object_is_visible(item));
        cave_set_feat(10,13,liquids[i]);assert(item->marked&&!object_is_visible(item));
        assert(cave_o_idx[10][13]==idx&&item->k_idx);
    }
    /* Exercise approach to an already seen tile through the real view update. */
    submerged_map();
    if(!view_g)view_g=calloc(VIEW_MAX,sizeof(*view_g));
    if(!temp_g)temp_g=calloc(TEMP_MAX,sizeof(*temp_g));
    view_n=0;assert(vinfo_init()==0);
    for(int y=0;y<32;y++)for(int x=0;x<32;x++)cave_info[y][x]|=CAVE_GLOW;
    cave_set_feat(10,13,FEAT_WATER);
    object_type obj=submerged_item(TV_GEM);int idx=floor_carry(10,13,&obj);
    p_ptr->skill_use[S_PER]=5;update_view();assert(cave_info[10][13]&CAVE_SEEN);
    assert(!o_list[idx].marked);
    p_ptr->px=12;update_view();assert(o_list[idx].marked&&object_is_visible(&o_list[idx]));
    p_ptr->px=10;update_view();assert(!object_is_visible(&o_list[idx]));
    p_ptr->skill_use[S_PER]=15;update_view();assert(object_is_visible(&o_list[idx]));
    p_ptr->blind=true;update_view();assert(!object_is_visible(&o_list[idx]));
    p_ptr->blind=false;forget_view();
    puts("Submerged items: perception thresholds, own square, blindness, sight, flood memory, bridge, map overlay and approach/retreat: PASS");

    const int terrain[]={FEAT_WATER,FEAT_DEEP_WATER,FEAT_LAVA,FEAT_POISON,
        FEAT_BRIDGE_WATER_H,FEAT_BRIDGE_WATER_V,FEAT_ICE};
    for(int t=0;t<7;t++)for(int gem=0;gem<2;gem++) {
        submerged_map();cave_set_feat(10,11,terrain[t]);
        obj=submerged_item(gem?TV_GEM:TV_SWORD);
        bool allowed=t>=4||(t<2&&gem);
        assert((floor_carry(10,11,&obj)>0)==allowed);
    }
    submerged_map();obj=submerged_item(TV_SWORD);idx=floor_carry(10,11,&obj);assert(idx>0);
    cave_set_feat(10,11,FEAT_DEEP_WATER);assert(cave_o_idx[10][11]==idx&&o_list[idx].k_idx);
    for(int gem=0;gem<2;gem++) {
        submerged_map();cave_set_feat(10,11,FEAT_WATER);
        obj=submerged_item(gem?TV_GEM:TV_SWORD);idx=drop_near(&obj,0,10,11);assert(idx>0);
        assert((cave_feat[o_list[idx].iy][o_list[idx].ix]==FEAT_WATER)==(gem!=0));
    }
    puts("Item generation: water gems only, no lava/poison spawns, bridges/ice, flooding survival and dry non-gem drops: PASS");
    submerged_map();p_ptr->py=p_ptr->px=2;p_ptr->skill_use[S_PER]=100;
    for(int y=6;y<=22;y++)for(int x=6;x<=26;x++) {
        cave_set_feat(y,x,(y==6||y==22||x==6||x==26)?FEAT_WATER:FEAT_DEEP_WATER);
        cave_info[y][x]|=CAVE_SEEN|CAVE_MARK;
    }
    for(int x=6;x<=26;x++)cave_set_feat(14,x,FEAT_BRIDGE_DEEP_WATER_H);
    obj=submerged_item(TV_GEM);assert(floor_carry(10,12,&obj)>0);
    water_preview("scripts/output/submerged-items-check/deep-water.png",2);
}

static void flood_trap_tests(void) {
    submerged_map();cave_flood_clear();
    cave_set_feat(10,10,FEAT_TRAP_FLOOD);
    cave_flood_trigger(10,10);
    assert(cave_feat[10][10]==FEAT_WATER&&cave_flood_stage_at(10,10)==1);
    /* The triggering action does not immediately expand the flood. */
    p_ptr->energy_use=100;cave_flood_end_action();
    assert(cave_feat[9][9]==FEAT_FLOOR&&cave_flood_stage_at(10,10)==1);
    /* One completed action floods radius one. */
    cave_flood_begin_action();cave_flood_end_action();
    assert(cave_feat[9][9]==FEAT_WATER&&cave_feat[8][10]==FEAT_FLOOR);
    /* The following action floods radius two and deepens the origin. */
    cave_flood_begin_action();cave_flood_end_action();
    assert(cave_feat[8][10]==FEAT_WATER&&cave_feat[10][10]==FEAT_DEEP_WATER);
    assert(cave_flood_stage_at(10,10)==0);
    puts("Flood trap: trigger, delayed radius-one spread, radius-two spread and deepened center: PASS");
}
'''


def main():
    water.OUT = water.ROOT / "scripts/output/submerged-items-check"
    water.TESTS = water.TESTS.replace("static void water_tests(void)",
                                    SUBMERGED_TESTS + "static void water_tests(void)")
    water.TESTS = water.TESTS.replace("vault_water_tests(); water_render_tests();",
                                    "vault_water_tests(); water_render_tests(); submerged_items_tests(); flood_trap_tests();")
    water.main()


if __name__ == "__main__":
    main()

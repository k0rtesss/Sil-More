#!/usr/bin/env python3
"""Focused production flood checks, independent of the authored vault fixture.

Build first. Uses isolated maps and the shared water/SDL harness, never saves.
"""
import check_water as water
import check_submerged_items as submerged

TESTS = r'''
static int flood_wet_count(void) {
    int count=0;
    for(int y=1;y<31;y++)for(int x=1;x<31;x++)
        if(cave_feat[y][x]==FEAT_WATER||cave_feat[y][x]==FEAT_DEEP_WATER)count++;
    return count;
}
static void flood_screenshot_tests(void) {
    /* Visible geometry from the report; S is the deep-water origin. Webs
     * and the alarm sit between the chamber and both exit doors. */
    static const char* layout[]={
        "###############",
        "#####..w+..+..#",
        "###....S####..#",
        "##.....#####..#",
        "#.a..#######..#",
        "##+#########..#",
        "#..#########..#",
        "#.............#",
        "###############"};
    submerged_map();cave_flood_clear();
    for(int y=1;y<31;y++)for(int x=1;x<31;x++)cave_set_feat(y,x,FEAT_WALL_EXTRA);
    int sy=0,sx=0;
    for(int y=0;y<(int)N_ELEMENTS(layout);y++)for(int x=0;layout[y][x];x++) {
        char c=layout[y][x];int feat=FEAT_FLOOR;
        if(c=='#')feat=FEAT_WALL_EXTRA;
        else if(c=='w')feat=FEAT_TRAP_WEB;
        else if(c=='a')feat=FEAT_TRAP_ALARM;
        else if(c=='+')feat=FEAT_DOOR_HEAD;
        else if(c=='S'){feat=FEAT_TRAP_FLOOD;sy=y+5;sx=x+5;}
        cave_set_feat(y+5,x+5,feat);
    }
    cave_flood_begin_action();cave_flood_trigger(sy,sx);
    p_ptr->energy_use=100;cave_flood_end_action();assert(flood_wet_count()==1);
    cave_flood_begin_action();cave_flood_end_action();assert(flood_wet_count()==9);
    cave_flood_begin_action();cave_flood_end_action();
    printf("Screenshot chamber: %d wet tiles after turn three\n",flood_wet_count());
    assert(flood_wet_count()==25);
    assert(cave_feat[sy][sx]==FEAT_DEEP_WATER);
    assert(cave_feat[6][12]==FEAT_WATER); /* northern web */
    assert(cave_feat[6][13]==FEAT_WATER); /* northern door */
    assert(cave_feat[9][7]==FEAT_WATER); /* southern alarm */
    assert(cave_feat[10][7]==FEAT_WATER); /* southern door */
    for(int y=0;y<(int)N_ELEMENTS(layout);y++)for(int x=0;layout[y][x];x++)
        if(layout[y][x]=='#')assert(cave_feat[y+5][x+5]==FEAT_WALL_EXTRA);
    puts("Screenshot chamber: both traps and exit doors destroyed; origin fixed; 25 wet tiles; walls intact: PASS");
}
static void flood_trap_target_tests(void) {
    for(int hidden=0;hidden<2;hidden++)
        for(int feat=FEAT_TRAP_HEAD;feat<=FEAT_TRAP_FLOOD;feat++) {
            if(!FEAT_IS_TRAP(feat))continue;
            for(int acid=0;acid<2;acid++) {
                submerged_map();cave_flood_clear();
                cave_set_feat(10,10,FEAT_TRAP_FLOOD);cave_set_feat(10,11,feat);
                /* A swallowed flood plate must not start a second event. */
                if(acid)cave_flood_set_trap_kind(10,11,CAVE_FLOOD_KIND_ACID);
                if(hidden)cave_info[10][11]|=CAVE_HIDDEN;
                cave_flood_begin_action();cave_flood_trigger(10,10);
                p_ptr->energy_use=100;cave_flood_end_action();
                cave_flood_begin_action();cave_flood_end_action();
                assert(flood_wet_count()==9&&cave_feat[10][11]==FEAT_WATER);
                assert(!(cave_info[10][11]&CAVE_HIDDEN));
                assert(!cave_flood_stage_at(10,11)&&!cave_flood_trap_kind_at(10,11));
                cave_flood_begin_action();cave_flood_end_action();assert(flood_wet_count()==25);
            }
        }
    puts("All visible/hidden trap targets destroyed, hidden flag cleared, water/acid plates removed without secondary floods: PASS");
}
static void flood_structure_tests(void) {
    const int structures[]={FEAT_OPEN,FEAT_BROKEN,FEAT_DOOR_HEAD,FEAT_DOOR_TAIL,
        FEAT_SECRET,FEAT_WARDED,FEAT_WARDED2,FEAT_WARDED3,FEAT_RUBBLE};
    for(int i=0;i<(int)N_ELEMENTS(structures)+FEAT_BRIDGE_TAIL-FEAT_BRIDGE_HEAD+1;i++) {
        int feat=i<(int)N_ELEMENTS(structures)?structures[i]
            :FEAT_BRIDGE_HEAD+i-N_ELEMENTS(structures);
        submerged_map();cave_flood_clear();
        cave_set_feat(10,10,FEAT_TRAP_FLOOD);cave_set_feat(10,11,feat);
        cave_flood_begin_action();cave_flood_trigger(10,10);
        p_ptr->energy_use=100;cave_flood_end_action();assert(flood_wet_count()==1);
        cave_flood_begin_action();p_ptr->energy_use=0;cave_flood_end_action();
        assert(flood_wet_count()==1);
        p_ptr->energy_use=100;cave_flood_end_action();
        assert(flood_wet_count()==9&&cave_feat[10][11]==FEAT_WATER);
        cave_flood_begin_action();cave_flood_end_action();assert(flood_wet_count()==25);
        cave_flood_begin_action();cave_flood_end_action();assert(flood_wet_count()==25);
    }
    /* A narrow corridor uses the full quota without touching its walls. */
    submerged_map();cave_flood_clear();
    for(int y=1;y<31;y++)for(int x=1;x<31;x++)
        cave_set_feat(y,x,y==10?FEAT_FLOOR:FEAT_WALL_EXTRA);
    cave_set_feat(10,15,FEAT_TRAP_FLOOD);
    cave_flood_begin_action();cave_flood_trigger(10,15);
    p_ptr->energy_use=100;cave_flood_end_action();assert(flood_wet_count()==1);
    cave_flood_begin_action();cave_flood_end_action();assert(flood_wet_count()==9);
    cave_flood_begin_action();cave_flood_end_action();assert(flood_wet_count()==25);
    for(int y=1;y<31;y++)for(int x=1;x<31;x++)
        if(y!=10)assert(cave_feat[y][x]==FEAT_WALL_EXTRA);
    /* A sealed pocket saturates rather than crossing a wall or its corner. */
    submerged_map();cave_flood_clear();
    for(int y=1;y<31;y++)for(int x=1;x<31;x++)
        cave_set_feat(y,x,FEAT_WALL_EXTRA);
    cave_set_feat(10,10,FEAT_TRAP_FLOOD);cave_set_feat(11,11,FEAT_FLOOR);
    cave_flood_begin_action();cave_flood_trigger(10,10);
    p_ptr->energy_use=100;cave_flood_end_action();
    for(int i=0;i<3;i++){cave_flood_begin_action();cave_flood_end_action();}
    assert(flood_wet_count()==1&&cave_feat[11][11]==FEAT_FLOOR);
    puts("All door/rubble/bridge variants, exact 1/9/25 totals, zero-energy actions, corridor quotas, sealed walls/corners and completion: PASS");
}
'''


def main():
    water.OUT = water.ROOT / "scripts/output/water-flood-check"
    water.TESTS = water.TESTS.replace("static void water_tests(void)",
        submerged.SUBMERGED_TESTS + TESTS + "static void water_tests(void)")
    water.TESTS = water.TESTS.replace(
        "    scent_tests(); movement_tests(); generation_tests(); monster_save_tests();\n"
        "    vault_water_tests(); water_render_tests();",
        "    flood_screenshot_tests(); flood_trap_target_tests(); flood_trap_tests(); acid_flood_trap_tests(); flood_structure_tests();")
    water.main()


if __name__ == "__main__":
    main()

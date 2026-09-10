#!/usr/bin/env python3
"""Exercise poison contact through production movement and action scheduling.

Build first. Uses an isolated SDL map; never opens player saves or config.
"""
import check_water as water
import re

TESTS = r'''
#include "melee/melee-process.h"
static int poison_hits;
/* This map harness supplies explicit bonuses without character/race tables. */
void __wrap_handle_stuff(void) {}
bool __wrap_get_check_near(int y,int x,cptr prompt) {
    (void)y;(void)x;(void)prompt;return picker_choice==0;
}
bool __wrap_mon_take_hit(int idx,int damage,cptr note,int source) {
    (void)note;(void)source;poison_hits++;
    monster_type* m=&mon_list[idx];m->hp-=damage;
    if(m->hp<=0) {cave_m_idx[m->fy][m->fx]=0;m->r_idx=0;return true;}
    return false;
}
static void poison_map(void) {
    water_test_reset_partition();water_map(32,32,FEAT_FLOOR);
    if(!c_info)c_info=calloc(1,sizeof(*c_info));
    p_ptr->pcharacter=0;p_ptr->poisoned=0;p_ptr->resist_pois=1;
    p_ptr->oppose_pois=0;p_ptr->leaving=false;picker_choice=0;
    p_ptr->confused=0;p_ptr->playing=true;cave_m_idx[10][10]=-1;
    p_ptr->active_ability[S_EVN][EVN_LEAPING]=false;
    memset(p_ptr->previous_action,0,sizeof(p_ptr->previous_action));
    p_ptr->energy_use=0;player_poison_terrain_end_action();
    monster_poison_terrain_end_action(1);
    mon_max=1;poison_hits=0;
    cave_set_feat(10,11,FEAT_POISON);
}
static void poison_step(int dir) {
    player_poison_terrain_begin_action();p_ptr->energy_use=100;
    move_player(dir);player_poison_terrain_end_action();
}
static monster_type* poison_monster(u32b f2,u32b f3) {
    poison_map();cave_m_idx[10][10]=0;p_ptr->py=p_ptr->px=25;
    cave_m_idx[25][25]=-1;
    memset(&mon_list[1],0,sizeof(mon_list[1]));
    memset(&r_info[1],0,sizeof(r_info[1]));
    memset(&l_list[1],0,sizeof(l_list[1]));
    monster_type* m=&mon_list[1];m->r_idx=1;m->fy=m->fx=10;
    m->hp=m->maxhp=200;m->alertness=ALERTNESS_ALERT;m->encountered=true;
    mon_max=2;mon_cnt=1;cave_m_idx[10][10]=1;
    r_info[1].name=1;r_info[1].cur_num=1;r_info[1].max_num=100;
    r_info[1].flags2=f2;r_info[1].flags3=f3;
    return m;
}
static void poison_terrain_tests(void) {
    const int doses[]={18,12,6,4,3};
    for(int res=-1;res<=3;res++) {
        poison_map();p_ptr->resist_pois=res;
        assert(player_poison_terrain_dose_at(10,11)==doses[res+1]);
        poison_step(6);
        assert(p_ptr->px==11&&p_ptr->poisoned==doses[res+1]);
        assert(p_ptr->chp==100); /* Contact is delayed poison, not a hit. */
    }
    poison_map();p_ptr->oppose_pois=1;poison_step(6);
    assert(p_ptr->poisoned==4);
    poison_map();poison_step(6);assert(p_ptr->poisoned==6);
    player_poison_terrain_begin_action();p_ptr->energy_use=100;
    player_poison_terrain_end_action();assert(p_ptr->poisoned==12);
    player_poison_terrain_begin_action();p_ptr->energy_use=0;
    player_poison_terrain_end_action();assert(p_ptr->poisoned==12);
    cave_set_feat(10,12,FEAT_POISON);poison_step(6);
    assert(p_ptr->poisoned==18);poison_step(6);
    assert(p_ptr->px==13&&p_ptr->poisoned==18);
    poison_map();picker_choice=1;poison_step(6);
    assert(p_ptr->px==10&&!p_ptr->energy_use&&!p_ptr->poisoned);
    assert(!sdl_mouse_path_grid_is_open_floor(10,11));
    assert(sdl_mouse_path_grid_is_known_danger(10,11));
    poison_map();p_ptr->active_ability[S_EVN][EVN_LEAPING]=true;
    p_ptr->previous_action[1]=6;poison_step(6);
    assert(p_ptr->leaping&&p_ptr->px==11&&!p_ptr->poisoned);
    player_poison_terrain_begin_action();continue_leap();
    player_poison_terrain_end_action();
    assert(!p_ptr->leaping&&p_ptr->px==12&&!p_ptr->poisoned);
    poison_map();p_ptr->active_ability[S_EVN][EVN_LEAPING]=true;
    p_ptr->previous_action[1]=6;poison_step(6);
    cave_set_feat(10,12,FEAT_WALL_EXTRA);
    player_poison_terrain_begin_action();continue_leap();
    player_poison_terrain_end_action();
    assert(!p_ptr->leaping&&p_ptr->px==11&&p_ptr->poisoned==6);
    poison_map();monster_swap(10,10,10,11);assert(p_ptr->poisoned==6);
    poison_map();character_dungeon=true;cave_set_feat(10,10,FEAT_POISON);
    assert(p_ptr->poisoned==6);character_dungeon=false;
    puts("Poison player terrain: resistance, temporary resistance, entry/wait/exit, cancellation, forced entry, creation, successful/blocked leaps: PASS");

    monster_type* m=poison_monster(0,0);
    monster_swap(10,10,10,11);assert(m->poisoned==6&&m->hp==200);
    monster_poison_terrain_begin_action(1);assert(m->poisoned==12);
    cave_set_feat(10,12,FEAT_POISON);monster_swap(10,11,10,12);
    assert(m->poisoned==12);
    assert(!monster_poison_tick(1));assert(m->poisoned==9&&m->hp==197);
    monster_poison_terrain_end_action(1);
    monster_swap(10,12,10,13);assert(m->poisoned==9);
    assert(!monster_poison_tick(1));assert(m->poisoned==7&&m->hp==195);
    for(int fly=0;fly<2;fly++) {
        m=poison_monster(fly?RF2_FLYING:0,fly?0:RF3_RES_POIS);
        monster_swap(10,10,10,11);monster_poison_terrain_begin_action(1);
        assert(!m->poisoned&&m->hp==200);monster_poison_terrain_end_action(1);
    }
    m=poison_monster(0,0);m->poisoned=10;m->skip_next_turn=true;m->energy=100;
    process_monsters(100);
    assert(m->hp==198&&m->poisoned==8&&m->energy==0&&!m->skip_next_turn);
    process_monsters(100);assert(m->hp==198&&poison_hits==1);
    m=poison_monster(0,0);m->poisoned=10;
    m->alertness=ALERTNESS_UNWARY-1;m->energy=100;
    process_monsters(100);assert(m->hp==198&&m->poisoned==8&&m->energy==0);
    m=poison_monster(0,0);cave_set_feat(10,10,FEAT_POISON);
    m->skip_next_turn=true;m->energy=100;process_monsters(100);
    assert(m->hp==198&&m->poisoned==4&&m->energy==0);
    puts("Poison monsters: forced contact, once per action, continued ticks off terrain, immunity/flight, sleeping/skipped turns and no-energy scheduling: PASS");
}
'''

def main():
    limits = (water.ROOT / "lib/edit/limits.txt").read_text(encoding="utf-8-sig")
    terrain = (water.ROOT / "lib/edit/terrain.txt").read_text(encoding="utf-8-sig")
    capacity = int(re.search(r"^M:F:(\d+)$", limits, re.M)[1])
    features = [int(n) for n in re.findall(r"^N:(\d+):", terrain, re.M)]
    assert max(features) < capacity, "Terrain IDs must fit the startup feature allocation"
    water.OUT = water.ROOT / "scripts/output/poison-terrain-check"
    water.TESTS = "static void poison_terrain_tests(void);\n" + water.TESTS.replace(
        "    vault_water_tests(); water_render_tests();",
        "    vault_water_tests(); water_render_tests(); poison_terrain_tests();") + TESTS
    original_run = water.subprocess.run

    def isolated_run(args, *pos, **kw):
        if args[0].endswith("cc.exe"):
            args = [*args, "-Wl,--wrap=get_check_near", "-Wl,--wrap=mon_take_hit",
                    "-Wl,--wrap=handle_stuff"]
        return original_run(args, *pos, **kw)

    water.subprocess.run = isolated_run
    water.main()

if __name__ == "__main__":
    main()

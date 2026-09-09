#!/usr/bin/env python3
"""Run lava gameplay through production movement and exposure code.

Extends the water SDL harness. Damage/death persistence is isolated from user
saves and scoring databases; movement, resistance, light, path and actor
scheduling remain production functions. Build with build-incremental.ps1 first.
"""
import check_water as water
import check_lava_monsters as monsters

TESTS = r'''
static int lava_hits, lava_deaths;
void __wrap_msg_format(cptr fmt,...) {(void)fmt;}
bool __wrap_get_check_near(int y,int x,cptr prompt) {
    (void)y;(void)x;assert(strstr(prompt,"lava"));return picker_choice==0;
}
void __wrap_take_hit(int damage,cptr reason) {
    (void)reason; lava_hits++;
    p_ptr->chp-=damage;
    if(p_ptr->chp<=0) {p_ptr->is_dead=true;p_ptr->leaving=true;}
}
void __wrap_monster_death(int m_idx) {
    lava_deaths++;
    assert(mon_list[m_idx].r_idx>0);
}
static void lava_player_map(int resistance) {
    water_map(32,32,FEAT_FLOOR);
    p_ptr->resist_fire=resistance;p_ptr->oppose_fire=0;
    p_ptr->is_dead=p_ptr->leaving=false;p_ptr->playing=true;
    p_ptr->confused=0;p_ptr->chp=p_ptr->mhp=500;
    p_ptr->active_ability[S_EVN][EVN_LEAPING]=false;
    cave_m_idx[10][10]=-1;
    cave_set_feat(10,11,FEAT_LAVA);
    mon_max=1;lava_hits=0;picker_choice=0;
}
static void lava_step(int dir) {
    player_lava_begin_action();p_ptr->energy_use=100;
    move_player(dir);player_lava_end_action();
}
static void lava_player_tests(void) {
    for(int res=-1;res<=7;res++) {
        lava_player_map(res);
        assert(player_lava_damage(false)==(res<=1 ? -1 : 120/(res+1)));
        assert(player_lava_damage(true)==(res+1<1 ? 60*(1-res) : 120/(res+2)));
        p_ptr->oppose_fire=10;
        assert(player_lava_damage(false)==(res+1<=1 ? -1 : 120/(res+2)));
    }
    puts("Lava resistance values: PASS");
    lava_player_map(1);lava_step(6);
    assert(p_ptr->px==11 && p_ptr->is_dead && lava_hits==1);
    lava_player_map(2);lava_step(6);
    assert(p_ptr->px==11 && p_ptr->chp==460 && lava_hits==1);
    player_lava_begin_action();p_ptr->energy_use=100;player_lava_end_action();
    assert(p_ptr->chp==420 && lava_hits==2);
    /* Free UI actions cost no exposure. A second lava tile is one hit. */
    player_lava_begin_action();p_ptr->energy_use=0;player_lava_end_action();
    assert(p_ptr->chp==420);
    cave_set_feat(10,12,FEAT_LAVA);lava_step(6);
    assert(p_ptr->chp==380 && lava_hits==3);
    lava_step(6);assert(p_ptr->px==13 && p_ptr->chp==380);

    /* Cancel either danger prompt without spending an action. */
    lava_player_map(1);picker_choice=1;lava_step(6);
    assert(p_ptr->px==10 && !p_ptr->energy_use && !lava_hits);
    lava_player_map(1);p_ptr->active_ability[S_EVN][EVN_LEAPING]=true;
    p_ptr->previous_action[1]=6;lava_step(6);
    assert(p_ptr->leaping && p_ptr->px==11 && p_ptr->chp==460 && lava_hits==1);
    player_lava_begin_action();continue_leap();player_lava_end_action();
    assert(!p_ptr->leaping && p_ptr->px==12 && p_ptr->chp==460 && lava_hits==1);
    /* A newly blocked landing is ground contact, immediately lethal. */
    lava_player_map(1);p_ptr->active_ability[S_EVN][EVN_LEAPING]=true;
    p_ptr->previous_action[1]=6;lava_step(6);
    cave_set_feat(10,12,FEAT_WALL_EXTRA);
    player_lava_begin_action();continue_leap();player_lava_end_action();
    assert(p_ptr->is_dead && p_ptr->px==11 && lava_hits==2);
    lava_player_map(3);p_ptr->active_ability[S_EVN][EVN_LEAPING]=true;
    p_ptr->previous_action[1]=6;lava_step(6);
    cave_set_feat(10,12,FEAT_WALL_EXTRA);
    player_lava_begin_action();continue_leap();player_lava_end_action();
    assert(!p_ptr->is_dead && p_ptr->chp==446 && lava_hits==2);
    /* Knockback cancels airborne protection before entering the lava. */
    lava_player_map(1);p_ptr->leaping=true;
    assert(knock_back(10,9,10,10));
    assert(p_ptr->is_dead && p_ptr->px==11 && !p_ptr->leaping);
    lava_player_map(1);
    assert(sdl_mouse_path_grid_is_known_danger(10,11));
    assert(!sdl_mouse_path_grid_walkable(10,11));
    assert(!sdl_mouse_path_grid_is_leapable_obstacle(10,11));
    assert(!cave_empty_bold(10,11) && !cave_clean_bold(10,11));
    /* An actual terrain change under the player applies entry immediately. */
    character_dungeon=true;cave_set_feat(10,10,FEAT_LAVA);
    assert(p_ptr->is_dead && lava_hits==1);character_dungeon=false;
    puts("Lava player: lethal contact, resistance tiers/potion, entry dedup, stationary/free turns, real and blocked leaps, knockback, safe paths, transformation: PASS");
}
static void lava_light_tests(void) {
    lava_player_map(2);
    for(int y=0;y<32;y++)for(int x=0;x<32;x++)cave_light[y][x]=0;
    lava_light();
    assert(cave_light[10][11]==3 && cave_light[10][10]==2);
    assert(cave_light[10][9]==1 && cave_light[10][8]==0);
    cave_set_feat(10,12,FEAT_LAVA);
    for(int y=0;y<32;y++)for(int x=0;x<32;x++)cave_light[y][x]=0;
    lava_light();assert(cave_light[10][11]==3 && cave_light[10][12]==3);
    cave_set_feat(10,12,FEAT_WALL_EXTRA);
    for(int y=0;y<32;y++)for(int x=0;x<32;x++)cave_light[y][x]=0;
    lava_light();assert(cave_light[10][13]==0 && cave_light[10][12]>0);
    cave_set_feat(10,11,FEAT_FLOOR);
    for(int y=0;y<32;y++)for(int x=0;x<32;x++)cave_light[y][x]=0;
    lava_light();assert(cave_light[10][10]==0);
    puts("Lava light: radius two, wall occlusion, overlapping pools capped, removal: PASS");
}
void lava_monster_tests(void);
static void lava_tests(void) {
    c_info=calloc(1,sizeof(*c_info));p_ptr->pcharacter=0;
    lava_player_tests();lava_light_tests();lava_monster_tests();
    assert(lava_deaths>=2);
}
'''


def main():
    water.OUT = water.ROOT / "scripts/output/lava-check"
    water.TESTS = water.TESTS.replace(
        "    vault_water_tests(); water_render_tests();",
        "    vault_water_tests(); water_render_tests(); lava_tests();")
    water.TESTS = "static void lava_tests(void);\n" + water.TESTS + TESTS + monsters.TESTS
    original_run = water.subprocess.run

    def run_with_isolated_death(args, *pos, **kw):
        if args[0].endswith("cc.exe"):
            args = [*args, "-Wl,--wrap=take_hit", "-Wl,--wrap=monster_death",
                    "-Wl,--wrap=get_check_near"]
            args += ["-Wl,--wrap=msg_format"]
        return original_run(args, *pos, **kw)

    water.subprocess.run = run_with_isolated_death
    water.main()


if __name__ == "__main__":
    main()

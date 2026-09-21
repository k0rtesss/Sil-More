#!/usr/bin/env python3
"""Exercise real illusion generation, movement and SDL pixels in an isolated map.

Build first with build-cmake.bat standard. Reuse the terrain regression harness
and its software renderer; no player saves or configuration files are opened.
"""
import check_water as water

TESTS = r'''
#include "cave/cave.h"
static bool same_illusion_cell(SDL_Surface* a,SDL_Surface* b,int big) {
    int x=(COL_MAP+11*(big?2:1))*16, y=(ROW_MAP+10)*16;
    for(int row=y;row<y+16;row++)
        if(memcmp((byte*)a->pixels+row*a->pitch+x*4,
                  (byte*)b->pixels+row*b->pitch+x*4,(big?32:16)*4)) return false;
    return true;
}
static int illusion_count(void) {
    int count = 0;
    for (int y=0; y<p_ptr->cur_map_hgt; y++)
        for (int x=0; x<p_ptr->cur_map_wid; x++)
            if (cave_feat[y][x] == FEAT_ILLUSORY_WALL) count++;
    return count;
}
static void illusion_corridor(int y, int x) {
    for (int dx=-1; dx<=1; dx++) {
        cave_set_feat(y, x+dx, FEAT_FLOOR);
        cave_corridor1[y][x+dx] = 0;
        cave_corridor2[y][x+dx] = 1;
    }
    cave_set_feat(y-1, x, FEAT_WALL_EXTRA);
    cave_set_feat(y+1, x, FEAT_WALL_EXTRA);
}
static void illusion_separator(int y, int x) {
    cave_set_feat(y, x, FEAT_WALL_EXTRA);
    cave_set_feat(y, x - 1, FEAT_WALL_EXTRA);
    cave_set_feat(y, x + 1, FEAT_WALL_EXTRA);
    cave_set_feat(y - 1, x, FEAT_FLOOR);
    cave_set_feat(y + 1, x, FEAT_FLOOR);
}
static void illusion_tests(void) {
    messages_init();
    assert(option_norm[OPT_illusory_walls]);
    assert(!strcmp(option_text[OPT_illusory_walls], "illusory_walls"));
    assert(OPT_illusory_walls < OPT_BIRTH);
    f_info[FEAT_ILLUSORY_WALL].mimic = FEAT_WALL_EXTRA;

    /* A single thin planned corridor, so placement has one unambiguous wall
     * pair. */
    for (int mode=0; mode<8; mode++) {
        water_map(24,24,FEAT_WALL_EXTRA);
        for(int y=0;y<24;y++) for(int x=0;x<24;x++)
            cave_info[y][x]=CAVE_WALL|CAVE_MARK|CAVE_SEEN;
        illusion_corridor(10,10);
        illusion_separator(16,16);
        op_ptr->opt[OPT_illusory_walls] = mode != 1;
        if(mode==2) cave_info[9][10] |= CAVE_ICKY;
        if(mode==3) cave_set_feat(9,10,FEAT_WALL_PERM);
        if(mode==4) p_ptr->depth=MORGOTH_DEPTH;
        if(mode==5) cave_fixture_set(9,10,CAVE_FIXTURE_WALL_TORCH);
        if(mode==6) cave_set_feat(10,10,FEAT_CHASM);
        if(mode==7) cave_set_feat(10,8,FEAT_DOOR_HEAD);
        place_illusory_passages();
        bool placed_pair = cave_feat[9][10]==FEAT_ILLUSORY_WALL
            || cave_feat[11][10]==FEAT_ILLUSORY_WALL;
        if (placed_pair != (mode==0))
            fprintf(stderr,"illusion generation mode %d: north %d south %d\n",
                mode, cave_feat[9][10], cave_feat[11][10]);
        assert(placed_pair == (mode==0));
        assert(cave_feat[10][9]==FEAT_FLOOR && cave_feat[10][11]==FEAT_FLOOR);
        assert(cave_feat[10][10] == (mode==6 ? FEAT_CHASM : FEAT_FLOOR));
        assert(cave_feat[16][16] != FEAT_ILLUSORY_WALL);
        if(mode==0) {
            assert(cave_floor_bold(9,10) || cave_floor_bold(11,10));
            assert(cave_floor_bold(9,10) != cave_floor_bold(11,10));
            assert(cave_feat[9][10]!=FEAT_ILLUSORY_WALL
                || cave_feat[11][10]==FEAT_WALL_EXTRA);
            assert(cave_feat[11][10]!=FEAT_ILLUSORY_WALL
                || cave_feat[9][10]==FEAT_WALL_EXTRA);
        }
    }
    puts("Illusion generation: default on, doorless corridors, option off, protected rooms, permanent walls, special depth, fixtures and hazards: PASS");

    /* The target count follows the map dimensions, like the stair count. */
    water_map(66,66,FEAT_FLOOR);
    illusion_corridor(10,10); illusion_corridor(20,20);
    illusion_corridor(30,30); illusion_corridor(40,40);
    op_ptr->opt[OPT_illusory_walls] = true;
    place_illusory_passages();
    int small_count = illusion_count();
    water_map(165,165,FEAT_FLOOR);
    illusion_corridor(10,10); illusion_corridor(30,30);
    illusion_corridor(50,50); illusion_corridor(70,70);
    illusion_corridor(90,90); illusion_corridor(110,110);
    illusion_corridor(130,130); illusion_corridor(150,150);
    place_illusory_passages();
    int large_count = illusion_count();
    assert(small_count == 2 && large_count == 8);
    puts("Illusion generation: map-size scaling: PASS");

    water_map(24,24,FEAT_FLOOR);
    cave_set_feat(10,11,FEAT_ILLUSORY_WALL);
    assert(cave_floor_bold(10,11) && !cave_known_closed_door_bold(10,11));
    assert(cave_monster_wall_bold(10,11));
    monster_type* m = &mon_list[1];
    memset(m, 0, sizeof(*m));
    m->r_idx = 1; m->fy = 10; m->fx = 10;
    cave_m_idx[10][10] = 1;
    u32b old_flags2 = r_info[1].flags2;
    r_info[1].flags2 = 0;
    bool bash = false;
    assert(!cave_exist_mon(&r_info[1], 10, 11, false, false));
    assert(cave_passable_mon(m, 10, 11, &bash) == 0);
    assert(monster_step_cost(m, 10, 10, 10, 11) == 0);
    r_info[1].flags2 = old_flags2;
    cave_m_idx[10][10] = 0;
    for (int light=-5; light<=10; light++) {
        cave_light[10][11]=light;
        assert(cave_illusion_opacity(10,11)==255-30*MIN(5,MAX(0,light)));
    }
    cave_info[10][11] &= ~CAVE_SEEN;
    assert(cave_illusion_opacity(10,11)==255);
    cave_info[10][11] |= CAVE_SEEN; p_ptr->blind=1;
    assert(cave_illusion_opacity(10,11)==255); p_ptr->blind=0;
    assert(cave_illusion_opacity(-1,11)==255);

    /* Walking in darkness costs one ordinary step; no door action is needed. */
    cave_light[10][11]=0; cave_m_idx[10][10]=-1;
    character_generated=false;
    p_ptr->energy_use=100; p_ptr->active_ability[S_EVN][EVN_LEAPING]=false;
    move_player(6);
    assert(p_ptr->px==11 && p_ptr->py==10 && p_ptr->energy_use==100);
    assert(cave_feat[10][11]==FEAT_FLOOR);
    cave_set_feat(10,12,FEAT_ILLUSORY_WALL);
    monster_swap(10,11,10,12);
    assert(p_ptr->px==12 && cave_feat[10][12]==FEAT_FLOOR);
    cave_set_feat(10,13,FEAT_ILLUSORY_WALL);
    wiz_light();
    assert(cave_feat[10][13]==FEAT_ILLUSORY_WALL);
    assert((cave_info[10][13] & (CAVE_GLOW|CAVE_MARK))==(CAVE_GLOW|CAVE_MARK));
    puts("Illusion rules: monotonic light response, darkness/blindness, walking and forced entry: PASS");

    /* Light-only updates must match a complete repaint, in both tile widths. */
    for (int big=0; big<2; big++) {
        water_map(24,24,FEAT_FLOOR); p_ptr->py=p_ptr->px=18;
        use_bigtile=big; character_generated=character_dungeon=true;
        cave_set_feat(10,11,FEAT_ILLUSORY_WALL);
        /* The real debug menu toggles dots on an unexplored tile, without
         * modifying its feature, visibility, lighting, or game RNG. */
        cave_info[10][11]=0;
        cave_light[10][11]=0;
        Term->total_erase=true; prt_map(); Term_fresh();
        SDL_Surface* unmarked=capture(90);
        u64b debug_rng=Rand_state_export();
        assert(!cave_illusion_debug_enabled());
        do_cmd_debug(); Term_fresh();
        assert(cave_illusion_debug_marked(10,11));
        assert(!cave_illusion_debug_marked(10,12));
        assert(cave_feat[10][11]==FEAT_ILLUSORY_WALL && !cave_info[10][11]
            && !cave_light[10][11] && debug_rng==Rand_state_export());
        SDL_Surface* marked=capture(91); assert(!same_surface(unmarked,marked));
        force_map_redraw(); Term_fresh();
        SDL_Surface* marked_full=capture(92); assert(same_surface(marked,marked_full));
        use_graphics=GRAPHICS_NONE;
        byte debug_a,debug_ta; char debug_c,debug_tc;
        map_info(10,11,&debug_a,&debug_c,&debug_ta,&debug_tc);
        assert(debug_c=='.' && debug_a==TERM_YELLOW);
        use_graphics=GRAPHICS_MICROCHASM;
        do_cmd_debug(); Term_fresh();
        assert(!cave_illusion_debug_enabled());
        SDL_Surface* cleared=capture(93); assert(same_surface(unmarked,cleared));
        SDL_DestroySurface(unmarked); SDL_DestroySurface(marked);
        SDL_DestroySurface(marked_full); SDL_DestroySurface(cleared);
        cave_info[10][11]=CAVE_MARK|CAVE_SEEN;
        cave_light[10][11]=1;
        Term->total_erase=true; prt_map(); Term_fresh();
        SDL_Surface* dim=capture(80);
        cave_light[10][11]=5; lite_spot(10,11); Term_fresh();
        SDL_Surface* bright=capture(81);
        assert(!same_surface(dim,bright));
        force_map_redraw(); Term_fresh();
        SDL_Surface* full=capture(82); assert(same_surface(bright,full));
        SDL_DestroySurface(dim); SDL_DestroySurface(bright); SDL_DestroySurface(full);
        cave_info[10][11] &= ~CAVE_SEEN; lite_spot(10,11); Term_fresh();
        SDL_Surface* remembered=capture(83);
        cave_light[10][11]=1; lite_spot(10,11); Term_fresh();
        SDL_Surface* hidden=capture(84); assert(same_surface(remembered,hidden));
        SDL_DestroySurface(remembered); SDL_DestroySurface(hidden);
        /* Unexplored and rage-hidden cells must not expose the floor layer. */
        for(int rage=0;rage<2;rage++) {
            cave_info[10][11]=rage ? CAVE_MARK : 0;
            p_ptr->rage=rage; force_map_redraw(); Term_fresh();
            SDL_Surface* unknown=capture(87);
            cave_feat[10][11]=FEAT_WALL_EXTRA;
            force_map_redraw(); Term_fresh();
            SDL_Surface* wall=capture(88); assert(same_illusion_cell(unknown,wall,big));
            SDL_DestroySurface(unknown); SDL_DestroySurface(wall);
            cave_feat[10][11]=FEAT_ILLUSORY_WALL;
        }
        p_ptr->rage=0; cave_info[10][11]=CAVE_MARK;
        force_map_redraw(); Term_fresh();
        cave_dissolve_illusion(10,11); Term_fresh();
        SDL_Surface* dissolved=capture(85); force_map_redraw(); Term_fresh();
        full=capture(86); assert(same_surface(dissolved,full));
        SDL_DestroySurface(dissolved); SDL_DestroySurface(full);
    }
    use_bigtile=false;
    water_map(16,30,FEAT_FLOOR); p_ptr->py=12;p_ptr->px=24;
    for(int x=3;x<27;x++) {
        cave_set_feat(5,x,FEAT_WALL_EXTRA);
        cave_set_feat(7,x,FEAT_WALL_EXTRA);
    }
    for(int light=0;light<=5;light++) {
        int x=4+light*4;
        cave_set_feat(6,x,FEAT_ILLUSORY_WALL); cave_light[6][x]=light;
    }
    water_preview("scripts/output/water-check/illusory-light-levels.png",3);
    cave_illusion_debug_set(true);
    water_preview("scripts/output/water-check/illusory-debug-dots.png",3);
    cave_illusion_debug_set(false);
    puts("Illusion debug: real menu toggle, dots on hidden tiles, ASCII, pixel cleanup, unchanged light/knowledge/RNG and wizard lighting: PASS");
    puts("Illusion SDL pixels: light blending, cached redraw, fog of war, dissolution and both tile widths: PASS");
}
'''

if __name__ == "__main__":
    water.TESTS = water.TESTS.replace("    assert(count == 2);", r'''
    char debug_key = !strcmp(title,"Debug Commands") ? '2'
        : !strcmp(title,"Map and Travel") ? 'i' : 0;
    if (debug_key) {
        for(int i=0;i<count;i++) if(options[i].key==debug_key) return i;
        assert(false);
    }
    assert(count == 2);''')
    water.TESTS = water.TESTS.replace("static void water_tests(void) {",
                                    TESTS + "\nstatic void water_tests(void) {")
    water.TESTS = water.TESTS.replace("vault_water_tests(); water_render_tests();",
                                    "vault_water_tests(); water_render_tests(); illusion_tests();")
    water.main()

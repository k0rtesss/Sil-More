#!/usr/bin/env python3
"""Exercise real illusion generation, movement and SDL pixels in an isolated map.

Build first with build-cmake.bat standard. Reuse the terrain regression harness
and its software renderer; no player saves or configuration files are opened.
"""
import check_water as water

TESTS = r'''
#include "cave/cave.h"
#include "level-generation/level-generation-terrain.h"
#include "level-generation/level-generation-terrain-access.h"
#include "level-generation/level-generation-terrain-vaults.h"

void apply_fixture_light_area(int y, int x, int radius, bool allow_room_tiles);

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
static void illusion_map(int size) {
    water_map(size, size, FEAT_WALL_EXTRA);
    terrain_generation_reset();
    terrain_vault_reset();
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            cave_info[y][x] = CAVE_WALL;
            cave_corridor1[y][x] = cave_corridor2[y][x] = -1;
        }
}
/* A strip of untouched granite separates two existing corridors. */
static void illusion_site(int y, int x, int dir, int thickness) {
    int dy = ddy[dir], dx = ddx[dir];
    for (int side = -1; side <= 1; side++) {
        int yy = y + side * dx, xx = x + side * dy;
        cave_set_feat(yy, xx, FEAT_FLOOR);
        cave_corridor1[yy][xx] = 0;
        cave_corridor2[yy][xx] = 1;
    }
    for (int i = thickness + 1; i <= thickness + 2; i++) {
        int yy = y + dy * i, xx = x + dx * i;
        cave_set_feat(yy, xx, FEAT_FLOOR);
        cave_corridor1[yy][xx] = 2;
        cave_corridor2[yy][xx] = 3;
    }
    /* The two outer landings are reachable before the secret is generated. */
    for (int i = 0; i <= thickness + 1; i++)
        cave_set_feat(y + dy*i + dx*2, x + dx*i + dy*2, FEAT_FLOOR);
    cave_set_feat(y + dy*(thickness+1) + dx, x + dx*(thickness+1) + dy, FEAT_FLOOR);
}
/* Exercise the game's eight-way movement graph with every illusion closed. */
static void illusion_access(int sy, int sx, bool sealed,
        byte reached[MAX_DUNGEON_HGT][MAX_DUNGEON_WID]) {
    static byte shadow[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    for (int y = 0; y < p_ptr->cur_map_hgt; y++)
        for (int x = 0; x < p_ptr->cur_map_wid; x++)
            shadow[y][x] = sealed && cave_feat[y][x] == FEAT_ILLUSORY_WALL
                ? FEAT_WALL_EXTRA : cave_feat[y][x];
    terrain_generation_flood(sy, sx, reached, shadow);
}

static void illusion_vault_tests(void) {
    vault_type templates[2] = {0};
    vault_type* old_templates = v_info;
    int old_max = z_info->v_max;
    v_info = templates;
    z_info->v_max = 2;
    static byte reached[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    for (int mode = 0; mode < 11; mode++) {
        illusion_map(32);
        templates[1].typ = mode == 2 ? 6 : mode == 3 ? 8 : 7;
        templates[1].flags = mode == 4 ? VLT_QUEST : 0;
        terrain_vault_begin();
        for (int y = 7; y <= 13; y++) for (int x = 12; x <= 18; x++) {
            bool edge = y == 7 || y == 13 || x == 12 || x == 18;
            cave_set_feat(y, x, edge ? FEAT_WALL_OUTER : FEAT_FLOOR);
            cave_info[y][x] |= CAVE_ROOM | CAVE_ICKY;
            terrain_vault_record(y, x, &templates[1], edge ? '%' : '.');
        }
        for (int y = 6; y <= 14; y++) for (int x = 7; x <= 11; x++)
            cave_set_feat(y, x, FEAT_FLOOR);
        cave_set_feat(10, 12, FEAT_FLOOR);
        cave_set_feat(10, 7, FEAT_LESS);
        if (mode == 1) cave_set_feat(10, 12, FEAT_DOOR_HEAD);
        if (mode == 5) cave_set_feat(7, 12, FEAT_FLOOR); /* Diagonal bypass. */
        if (mode == 6) cave_set_feat(8, 12, FEAT_CHASM); /* Jumpable bypass. */
        if (mode == 7) cave_set_feat(11, 15, FEAT_MORE);
        if (mode == 8) cave_set_feat(11, 15, FEAT_FORGE_NORMAL_HEAD);
        if (mode == 9) p_ptr->px = 15;
        op_ptr->opt[OPT_illusory_walls] = mode != 10;
        place_illusory_passages();
        assert(illusion_count() == (mode <= 1 ? 1 : 0));
        if (mode <= 1) {
            assert(cave_feat[10][12] == FEAT_ILLUSORY_WALL);
            illusion_access(10, 10, true, reached);
            assert(reached[10][7]);
            for (int y = 8; y <= 12; y++) for (int x = 13; x <= 17; x++)
                assert(!reached[y][x]);
            illusion_access(10, 10, false, reached);
            assert(reached[10][17]);
        }
    }
    terrain_vault_reset();
    v_info = old_templates;
    z_info->v_max = old_max;
    op_ptr->opt[OPT_illusory_walls] = true;
    puts("Illusion generation: optional vault accessible only through its single threshold: PASS");
}

static void illusion_light_tests(void) {
    static u16b view_grids[VIEW_MAX], temp_grids[TEMP_MAX];
    view_g = view_grids;
    temp_g = temp_grids;
    assert(vinfo_init() == 0);
    illusion_map(24);
    for (int x = 9; x <= 14; x++) cave_set_feat(10, x, FEAT_FLOOR);
    cave_set_feat(10, 11, FEAT_ILLUSORY_WALL);
    cave_m_idx[10][10] = -1;

    p_ptr->cur_light = 4;
    update_view();
    assert(player_can_see_bold(10, 11) && cave_illusion_opacity(10, 11) < 255);
    assert(!(cave_info[10][12] & (CAVE_VIEW | CAVE_SEEN | CAVE_MARK)));
    assert(cave_light[10][12] == 0);
    assert(los(10, 10, 10, 11));
    assert(!los(10, 10, 10, 12) && !los(10, 12, 10, 10));

    p_ptr->cur_light = 0;
    cave_set_feat(10, 12, FEAT_LAVA);
    update_view();
    assert(cave_light[10][12] > 0);
    assert(cave_light[10][10] == 0 && cave_light[10][11] == 0);
    assert(cave_illusion_opacity(10, 11) == 255);

    cave_set_feat(10, 12, FEAT_FLOOR);
    mon_list[1] = (monster_type){ .r_idx = 1, .fy = 10, .fx = 12 };
    r_info[1].light = 3;
    mon_max = 2;
    update_view();
    assert(cave_light[10][10] == 0 && cave_light[10][11] == 0);
    assert(cave_illusion_opacity(10, 11) == 255);
    mon_list[1].fx = 9;
    update_view();
    assert(player_can_see_bold(10, 11) && cave_illusion_opacity(10, 11) < 255);
    assert(cave_light[10][12] == 0);
    r_info[1].light = 0;
    mon_list[1].r_idx = 0;
    mon_max = 1;

    /* The fixture lights the far passage, which becomes visible on dissolution. */
    apply_fixture_light_area(10, 12, 2, false);
    update_view();
    assert(cave_light[10][12] > 0 && !player_can_see_bold(10, 12));
    assert(cave_light[10][10] == 0 && cave_illusion_opacity(10, 11) == 255);
    character_dungeon = true;
    p_ptr->update = 0;
    cave_dissolve_illusion(10, 11);
    assert(p_ptr->update & PU_UPDATE_VIEW);
    update_view();
    assert(los(10, 10, 10, 12) && player_can_see_bold(10, 12));
    forget_view();
    character_dungeon = false;
    puts("Illusion occlusion: sight, player, monster, lava and fixture light, and dissolution: PASS");
}

static void illusion_tests(void) {
    messages_init();
    assert(option_norm[OPT_illusory_walls]);
    assert(!strcmp(option_text[OPT_illusory_walls], "illusory_walls"));
    assert(OPT_illusory_walls < OPT_BIRTH);
    f_info[FEAT_ILLUSORY_WALL].mimic = FEAT_WALL_EXTRA;
    op_ptr->opt[OPT_illusory_walls] = true;

    /* Reproduce the reported blind alcove and other invalid routes. No
     * partial excavation may survive a failed search for the far landing. */
    for (int mode = 0; mode < 20; mode++) {
        illusion_map(24);
        int thickness = mode == 13 ? 9 : (mode >= 16 && mode <= 18) ? mode - 15 : 4;
        illusion_site(10, 10, 6, thickness);
        op_ptr->opt[OPT_illusory_walls] = mode != 1;
        if (mode == 0) {
            cave_set_feat(10, 15, FEAT_WALL_EXTRA);
            cave_set_feat(10, 16, FEAT_WALL_EXTRA);
        }
        if (mode == 2) cave_info[9][13] |= CAVE_ICKY;
        if (mode == 3) cave_set_feat(10, 13, FEAT_WALL_PERM);
        if (mode == 4) p_ptr->depth = MORGOTH_DEPTH;
        if (mode == 5) cave_fixture_set(10, 14, CAVE_FIXTURE_WALL_TORCH);
        if (mode == 6) cave_set_feat(10, 15, FEAT_CHASM);
        if (mode == 7) cave_set_feat(8, 10, FEAT_DOOR_HEAD);
        if (mode == 8) cave_set_feat(10, 16, FEAT_WALL_EXTRA);
        if (mode == 9) cave_set_feat(9, 13, FEAT_FLOOR);
        if (mode == 10) cave_o_idx[10][12] = 1;
        if (mode == 11) cave_info[10][15] |= CAVE_ROOM;
        if (mode == 12) terrain_generation_reserve(10, 13);
        if (mode == 14) cave_natural[10][15] = 1;
        if (mode == 15) cave_set_feat(9, 11, FEAT_FLOOR); /* Diagonal entry around the mouth. */
        if (mode == 19) cave_set_feat(11, 15, FEAT_WALL_EXTRA); /* Cut the exterior route. */
        place_illusory_passages();
        assert(illusion_count() == 0);
        for (int i = 11; i <= 10 + thickness; i++)
            assert(cave_feat[10][i] ==
                (mode == 3 && i == 13 ? FEAT_WALL_PERM : FEAT_WALL_EXTRA));
    }
    puts("Illusion generation: blind alcoves, side openings, hazards and protected routes rejected: PASS");

    static byte reached[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    for (int d = 0; d < 4; d++) for (int thickness = 4; thickness <= 8; thickness++) {
        int dir = ddd[d], dy = ddy[dir], dx = ddx[dir];
        illusion_map(32);
        illusion_site(14, 14, dir, thickness);
        int far_y = 14 + dy*(thickness+1), far_x = 14 + dx*(thickness+1);
        p_ptr->py = p_ptr->px = 14;
        place_illusory_passages();
        assert(illusion_count() == 2);
        illusion_access(14, 14, true, reached);
        assert(reached[far_y][far_x]);
        for (int i = 1; i <= thickness; i++)
            assert(!reached[14 + dy*i][14 + dx*i]);
        cave_m_idx[14][14] = -1;
        for (int i = 1; i <= thickness + 2; i++) {
            int y = 14 + dy*i, x = 14 + dx*i;
            bool entrance = i == 1 || i == thickness;
            assert(cave_feat[y][x] == (entrance ? FEAT_ILLUSORY_WALL : FEAT_FLOOR));
            move_player(dir);
            assert(p_ptr->py == y && p_ptr->px == x && cave_feat[y][x] == FEAT_FLOOR);
        }
    }
    puts("Illusion generation: sealed corridors exclude eight-way access until entered: PASS");

    for (int size = 66; size <= 165; size += 99) {
        illusion_map(size);
        for (int y = 10; y < size - 7; y++) cave_set_feat(y, 8, FEAT_FLOOR);
        for (int y = 10; y < size - 7; y += 8) {
            illusion_site(y, 10, 6, 4);
            cave_set_feat(y, 9, FEAT_FLOOR);
        }
        place_illusory_passages();
        assert(illusion_count() == (size == 66 ? 2 : 8));
    }
    puts("Illusion generation: map-size scaling: PASS");
    illusion_vault_tests();
    illusion_light_tests();

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

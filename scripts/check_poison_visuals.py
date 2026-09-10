#!/usr/bin/env python3
"""Exercise poison through the production SDL liquid renderer (build first)."""
import check_water as water

GENERATION = r'''
void poison_generation_tests(void) {
    static dun_data dungeon;
    dun=&dungeon;
    for(int seed=1;seed<=100;seed++) {
        water_map(64,96,FEAT_WALL_EXTRA);memset(dun,0,sizeof(*dun));
        memset(room_anchor_kind,0,sizeof(room_anchor_kind));
        Rand_state_init(seed);layout_anchor_count=0;
        current_partition_modes[0]=QUAD_MODE_BIG_CAVE;
        current_partition_big_cave_types[0]=BIG_CAVE_POIS;
        assert(carve_big_cave_bounds(2,61,2,93,0,BIG_CAVE_POIS));
        place_cave_poison();
        int count=0;
        for(int y=1;y<63;y++)for(int x=1;x<95;x++)
            count+=cave_feat[y][x]==FEAT_POISON;
        assert(count>=4&&count<=30);
        if(seed==1)water_preview("scripts/output/poison-visual-check/cave-poison.png",1);
    }
    puts("Poison terrain: 100 production big-cave layouts contain small seeps: PASS");
}
'''

PIXELS = r'''
static void poison_frame_pixels(SDL_Surface* canvas, int frame) {
    char path[160];
    strnfmt(path,sizeof(path),"lib/xtra/graf/anim_lava_flow_f%d.png",frame);
    SDL_Surface* source=IMG_Load(path);assert(source);
    for(int y=0;y<16;y++)for(int x=0;x<16;x++) {
        Uint8 r,g,b,a,pr,pg,pb,pa;
        assert(SDL_ReadSurfacePixel(source,x,y,&r,&g,&b,&a));
        assert(SDL_ReadSurfacePixel(canvas,(COL_MAP+11)*16+x,
            (ROW_MAP+10)*16+y,&pr,&pg,&pb,&pa));
        assert(pr==g/2&&pg==r&&pb==b&&pa==a);
    }
    SDL_DestroySurface(source);
}
'''


def main():
    water.OUT = water.ROOT / "scripts/output/poison-visual-check"
    start = water.TESTS.index("static void water_render_tests(void)")
    end = water.TESTS.index("static void water_tests(void)")
    render = water.TESTS[start:end].replace("water_render_tests", "poison_render_tests")
    render = render.replace("FEAT_WATER", "FEAT_POISON").replace("water_texture", "poison_texture")
    render = render.replace("frames[i]=capture(40+i);",
                            "frames[i]=capture(40+i);poison_frame_pixels(frames[i],i);")
    render = render.replace("Water pixels:", "Poison exact recolored lava pixels:")
    render = render.replace("    for(int i=0;i<4;i++)SDL_DestroySurface(frames[i]);", r'''
    cave_info[10][12]=CAVE_MARK;
    assert(visible_liquid(10,12)==FEAT_POISON);
    p_ptr->rage=true;assert(!visible_liquid(10,12));p_ptr->rage=false;
    g_labyrinth_view_active=true;assert(!visible_liquid(10,12));g_labyrinth_view_active=false;
    p_ptr->blind=true;expect_paused(40*IDLE_STEP_NS);p_ptr->blind=false;
    for(int i=0;i<4;i++)SDL_DestroySurface(frames[i]);
    sdl_idle_animation_shutdown();assert(!poison_texture&&!poison_load_attempted);
    assert(load_liquid_texture(FEAT_POISON));
''')
    water.TESTS = water.TESTS.replace("static void water_tests(void)",
                                    PIXELS + render + "static void water_tests(void)")
    water.TESTS = water.TESTS.replace("static void water_render_tests(void)",
                                    GENERATION + "static void water_render_tests(void)")
    water.TESTS = "void poison_generation_tests(void);\n" + water.TESTS
    water.TESTS = water.TESTS.replace("vault_water_tests(); water_render_tests();",
                                    "vault_water_tests(); water_render_tests(); poison_generation_tests(); poison_render_tests();")
    water.main()


if __name__ == "__main__":
    main()

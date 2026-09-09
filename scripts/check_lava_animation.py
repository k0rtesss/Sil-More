#!/usr/bin/env python3
"""Exercise production lava rendering with the software SDL fixture harness.

Run after build-incremental.ps1. Uses isolated maps and no player saves.
"""
import check_idle_animation as idle


TESTS = r'''
static bool lava_cell_changed(SDL_Surface* a, SDL_Surface* b) {
    for(int row=0;row<16;row++)
        if(memcmp((byte*)a->pixels+((ROW_MAP+10)*16+row)*a->pitch+(COL_MAP+11)*16*4,
                  (byte*)b->pixels+((ROW_MAP+10)*16+row)*b->pitch+(COL_MAP+11)*16*4,
                  16*4)) return true;
    return false;
}
static void lava_scene_preview(void) {
    const int h=18,w=30,scale=2;
    p_ptr->cur_map_hgt=h; p_ptr->cur_map_wid=w;
    style_info[0].wall_row=16; style_info[0].wall_col=22;
    style_info[0].floor_row=23; style_info[0].floor_col=6;
    for(int y=0;y<h;y++) for(int x=0;x<w;x++) {
        bool wall=y<2||y>=h-2||x<2||x>=w-2;
        bool pool=(x-9)*(x-9)+2*(y-9)*(y-9)<24;
        int river=20+(y/3)%3;
        cave_feat[y][x]=wall?FEAT_WALL_EXTRA:
            (pool||x==river||x==river+1)?FEAT_LAVA:FEAT_FLOOR;
        cave_info[y][x]=CAVE_MARK|CAVE_SEEN|CAVE_GLOW|(wall?CAVE_WALL:0);
        cave_m_idx[y][x]=0; cave_light[y][x]=2;
    }
    SDL_Texture* previous=SDL_GetRenderTarget(g_state.renderer);
    SDL_Texture* target=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,w*16*scale,h*16*scale);
    assert(target); SDL_SetRenderTarget(g_state.renderer,target);
    SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255); SDL_RenderClear(g_state.renderer);
    for(int y=0;y<h;y++) for(int x=0;x<w;x++) {
        byte a,ta; char c,tc;
        map_info(y,x,&a,&c,&ta,&tc);
        SDL_FRect dst={x*16*scale,y*16*scale,16*scale,16*scale};
        sdl_draw_map_tile_layers_at(y,x,a,c,ta,tc,&dst);
    }
    SDL_Surface* surface=SDL_RenderReadPixels(g_state.renderer,NULL); assert(surface);
    assert(IMG_SavePNG(surface,"scripts/output/lava-animation-check/lava-scene.png"));
    SDL_DestroySurface(surface); SDL_SetRenderTarget(g_state.renderer,previous);
    SDL_DestroyTexture(target);
}
static void lava_render_tests(void) {
    cave_fixtures_clear();
    sdl_idle_animation_clear_cells();
    p_ptr->wx=p_ptr->wy=0; p_ptr->blind=p_ptr->rage=0;
    Term->soft_cursor=false; use_bigtile=false;
    for(int y=0;y<32;y++) for(int x=0;x<32;x++) {
        cave_feat[y][x]=FEAT_FLOOR;
        cave_info[y][x]=CAVE_MARK|CAVE_SEEN;
        cave_m_idx[y][x]=0; cave_o_idx[y][x]=0;
    }
    f_info[FEAT_LAVA].x_char=(char)(TILE_FLAG|1);
    f_info[FEAT_WATER].x_char=(char)(TILE_FLAG|1);
    cave_feat[10][11]=FEAT_LAVA; cave_feat[10][12]=FEAT_WATER;
    Term->total_erase=true; prt_map(); Term_fresh();
    assert(lava_texture && water_texture && cell_count==2);
    u64b rng=Rand_state_export(); s32b turns=turn; s16b energy=p_ptr->energy;
    int loads=image_loads, textures=texture_creations, mallocs=allocations;
    SDL_Surface* frames[4];
    for(int frame=0;frame<4;frame++) {
        frame_tick=(Uint64)frame*8; force_map_redraw(); Term_fresh();
        frames[frame]=capture(60+frame);
        char asset[128];
        strnfmt(asset,sizeof(asset),"lib/xtra/graf/anim_lava_flow_f%d.png",frame);
        expect_middle_frame(frames[frame],10,11,asset);
        if(frame) assert(!same_surface(frames[frame-1],frames[frame]));
    }
    assert(sdl_idle_animation_timeout_ms(32*IDLE_STEP_NS)==0);
    sdl_idle_animation_update(32*IDLE_STEP_NS);
    assert(image_loads==loads && texture_creations==textures && allocations==mallocs);
    assert(Rand_state_export()==rng && turn==turns && p_ptr->energy==energy);
    /* Transparent actors must preserve the liquid animation beneath them. */
    draw_cell(10,11,true);
    const idle_cell* occupied=NULL;
    for(int i=0;i<cell_count;i++) if(cells[i].x==11) occupied=&cells[i];
    assert(occupied && cell_can_animate(occupied));
    SDL_Surface* before=capture(64);
    sdl_idle_animation_update(40*IDLE_STEP_NS);
    SDL_Surface* after=capture(65);
    assert(lava_cell_changed(before,after));
    SDL_DestroySurface(before); SDL_DestroySurface(after);
    /* Both tile widths and pans must repaint identically to a fresh map. */
    for(int big=0;big<2;big++) {
        use_bigtile=big; Term->total_erase=true;
        for(int pan=0;pan<3;pan++) {
            p_ptr->wx=pan; prt_map(); Term_fresh();
            before=capture(66); force_map_redraw(); Term_fresh(); after=capture(67);
            assert(same_surface(before,after));
            SDL_DestroySurface(before); SDL_DestroySurface(after);
        }
    }
    use_bigtile=false; p_ptr->wx=0; Term->total_erase=true; prt_map(); Term_fresh();
    cave_info[10][11]&=~CAVE_SEEN; cave_info[10][12]&=~CAVE_SEEN;
    for(int option=0;option<2;option++) {
        op_ptr->opt[OPT_torch_animation_always]=option;
        frame_tick=48; force_map_redraw(); Term_fresh(); before=capture(68);
        frame_tick=56; force_map_redraw(); Term_fresh(); after=capture(69);
        assert(same_surface(before,after));
        assert(sdl_idle_animation_timeout_ms(64*IDLE_STEP_NS)==-1);
        SDL_DestroySurface(before); SDL_DestroySurface(after);
    }
    cave_info[10][11]=0; assert(!visible_liquid(10,11));
    cave_info[10][11]=CAVE_MARK; p_ptr->rage=1;
    assert(!visible_liquid(10,11)); p_ptr->rage=0;
    cave_info[10][11]=CAVE_MARK|CAVE_SEEN;
    p_ptr->blind=1; expect_paused(64*IDLE_STEP_NS); p_ptr->blind=0;
    cave_set_feat(10,11,FEAT_FLOOR); Term_fresh(); before=capture(70);
    force_map_redraw(); Term_fresh(); after=capture(71);
    assert(same_surface(before,after));
    SDL_DestroySurface(before); SDL_DestroySurface(after);
    for(int i=0;i<4;i++) SDL_DestroySurface(frames[i]);
    lava_scene_preview();
    puts("Lava pixels: four original frames, liquid under actors, water coexistence, pan/erase, fog and blindness, no turns/RNG/idle allocations: PASS");
}
'''


def main():
    idle.OUT = idle.ROOT / "scripts/output/lava-animation-check"
    idle.HARNESS = idle.HARNESS.replace(
        "scripts/output/idle-animation-check", "scripts/output/lava-animation-check")
    idle.HARNESS = idle.HARNESS.replace("int main(void) {", TESTS + "\nint main(void) {")
    idle.HARNESS = idle.HARNESS.replace(
        "    asynchronous_tests();", "    asynchronous_tests();\n    lava_render_tests();")
    idle.main()


if __name__ == "__main__":
    main()

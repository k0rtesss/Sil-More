#!/usr/bin/env python3
"""Production SDL calm shoal/sea/acid transitions and single-bank checks.

Build first; isolated maps and source template parsing never open player saves.
"""
import check_idle_animation as idle
import check_cave_floor_tiles as floors
import check_terrain_transitions as shores

TESTS = r'''
static const int wet_features[3]={FEAT_WATER,FEAT_DEEP_WATER,FEAT_POISON};
static SDL_Surface* wet_expected(SDL_Texture* texture,int mask,int frame,int light,bool deep_fallback) {
    SDL_Texture* previous=SDL_GetRenderTarget(g_state.renderer);
    SDL_Texture* target=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,16,16); assert(target);
    SDL_SetRenderTarget(g_state.renderer,target);
    SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255);SDL_RenderClear(g_state.renderer);
    SDL_FRect dst={0,0,16,16},src={mask<0?frame*16:(mask%16)*16,
        mask<0?0:(mask/16+frame*16)*16,16,16};
    SDL_SetTextureColorMod(texture,deep_fallback?80*light/255:light,
        deep_fallback?105*light/255:light,deep_fallback?205*light/255:light);
    assert(SDL_RenderTexture(g_state.renderer,texture,&src,&dst));
    SDL_Surface* result=SDL_RenderReadPixels(g_state.renderer,NULL);assert(result);
    SDL_SetRenderTarget(g_state.renderer,previous);SDL_DestroyTexture(target);return result;
}
static void wet_atlas_tests(void) {
    u64b rng=Rand_state_export();s32b turns=turn;
    for(int kind=0;kind<3;kind++) {
        int feat=wet_features[kind];assert(load_liquid_texture(feat));
        SDL_Texture* atlas=load_liquid_transition_texture(feat);assert(atlas);
        int count=feat==FEAT_POISON?3:4;
        assert(liquid_frame_count(feat)==count);
        for(int mask=0;mask<256;mask++) {
            shore_map(feat,mask);assert(liquid_transition_mask(10,11,feat)==mask);
            for(int frame=0;frame<count;frame++) {
                SDL_Surface* actual=shore_surface(NULL,mask,frame,true);
                SDL_Surface* expected=wet_expected(atlas,normalized_shore(mask),frame,255,false);
                assert(same_surface(actual,expected));SDL_DestroySurface(actual);SDL_DestroySurface(expected);
            }
        }
        shore_map(feat,255);
        SDL_Surface* frames[4];
        for(int frame=0;frame<count;frame++) frames[frame]=shore_surface(NULL,255,frame,true);
        if(feat!=FEAT_POISON)for(int frame=0;frame<4;frame++) {
            char source[256];int stages[4]={0,1,2,1};
            strnfmt(source,sizeof(source),"C:/Assets/verdant-04-tidewater-tileset/tiles/16x16/anim_%s_f%d.png",
                feat==FEAT_WATER?"shoal":"sea",stages[frame]);
            SDL_Surface* native=IMG_Load(source);
            if(!native) {puts("Native purchased pack absent; source comparison skipped (atlas/render checks still run)");break;}
            assert(native->w==16&&native->h==16);
            SDL_Texture* original=SDL_CreateTextureFromSurface(g_state.renderer,native);assert(original);
            SDL_Surface* expected=wet_expected(original,-1,0,255,false);
            assert(same_surface(frames[frame],expected));SDL_DestroySurface(expected);
            SDL_DestroyTexture(original);SDL_DestroySurface(native);
        }
        assert(!same_surface(frames[0],frames[1])&&!same_surface(frames[1],frames[2]));
        if(count==4)assert(same_surface(frames[1],frames[3]));
        SDL_Surface* wrapped=shore_surface(NULL,255,count,true);assert(same_surface(frames[0],wrapped));
        SDL_DestroySurface(wrapped);for(int frame=0;frame<count;frame++)SDL_DestroySurface(frames[frame]);
        cave_info[10][11]=CAVE_MARK;
        SDL_Surface* dark=shore_surface(NULL,255,2,true);
        SDL_Surface* expected=wet_expected(atlas,255,0,96,false);
        assert(same_surface(dark,expected));SDL_DestroySurface(dark);SDL_DestroySurface(expected);
    }
    assert(Rand_state_export()==rng&&turn==turns);
    puts("Shoal/sea/acid: all256 masks, calm 0-1-2-1 water and three-frame acid, wraparound and dark pixels without extra deep tint: PASS");
}
static SDL_Surface* wet_mixed_expected(SDL_Texture* depth,SDL_Texture* bank,int depthmask,int bankmask,int frame) {
    SDL_Texture* previous=SDL_GetRenderTarget(g_state.renderer);
    SDL_Texture* target=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,16,16);assert(target);
    SDL_SetRenderTarget(g_state.renderer,target);
    SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255);SDL_RenderClear(g_state.renderer);
    SDL_FRect dst={0,0,16,16};
    SDL_FRect src={(depthmask%16)*16,(depthmask/16+frame*16)*16,16,16};
    SDL_SetTextureColorMod(depth,255,255,255);assert(SDL_RenderTexture(g_state.renderer,depth,&src,&dst));
    src=(SDL_FRect){(bankmask%16)*16,(bankmask/16+frame*16)*16,16,16};
    SDL_SetTextureColorMod(bank,255,255,255);assert(SDL_RenderTexture(g_state.renderer,bank,&src,&dst));
    SDL_Surface* result=SDL_RenderReadPixels(g_state.renderer,NULL);assert(result);
    SDL_SetRenderTarget(g_state.renderer,previous);SDL_DestroyTexture(target);return result;
}
static void wet_depth_tests(void) {
    shore_map(FEAT_WATER,255);cave_feat[9][11]=FEAT_DEEP_WATER;
    SDL_Surface* loaded=shore_surface(NULL,0,0,true);SDL_DestroySurface(loaded);
    assert(water_depth_transition_texture&&water_bank_overlay_texture);
    u64b rng=Rand_state_export();s32b turns=turn;
    for(int mask=0;mask<255;mask++) {
        shore_map(FEAT_WATER,255);
        for(int i=0;i<8;i++)if(!(mask&(1<<i)))cave_feat[10+shore_dy[i]][11+shore_dx[i]]=FEAT_DEEP_WATER;
        assert(water_depth_transition_mask(10,11)==mask);
        for(int frame=0;frame<4;frame++) {
            SDL_Surface* actual=shore_surface(NULL,mask,frame,true);
            SDL_Surface* expected=wet_mixed_expected(water_depth_transition_texture,water_bank_overlay_texture,mask,255,frame);
            assert(same_surface(actual,expected));SDL_DestroySurface(actual);SDL_DestroySurface(expected);
        }
    }
    /* Shoreline and depth edges can occupy the same shallow tile, including diagonals. */
    for(int bankmask=1;bankmask<256;bankmask++) {
        shore_map(FEAT_WATER,bankmask);int deepbits=bankmask&0xAA;
        if(!deepbits)deepbits=bankmask&0x55;
        for(int i=0;i<8;i++)if(deepbits&(1<<i))cave_feat[10+shore_dy[i]][11+shore_dx[i]]=FEAT_DEEP_WATER;
        for(int frame=0;frame<4;frame++) {
            SDL_Surface* actual=shore_surface(NULL,0,frame,true);
            SDL_Surface* expected=wet_mixed_expected(water_depth_transition_texture,water_bank_overlay_texture,255^deepbits,bankmask,frame);
            assert(same_surface(actual,expected));SDL_DestroySurface(actual);SDL_DestroySurface(expected);
        }
    }
    shore_map(FEAT_WATER,255);cave_feat[9][11]=FEAT_BRIDGE_DEEP_WATER_H;
    assert(water_depth_transition_mask(10,11)==254);cave_info[9][11]=0;
    SDL_Surface* hidden=shore_surface(NULL,0,0,true);
    assert(water_depth_transition_mask(10,11)==255);
    cave_feat[9][11]=FEAT_FLOOR;SDL_Surface* absent=shore_surface(NULL,0,0,true);
    assert(same_surface(hidden,absent));SDL_DestroySurface(hidden);SDL_DestroySurface(absent);
    cave_feat[9][11]=FEAT_DEEP_WATER;cave_info[9][11]=CAVE_MARK;
    assert(water_depth_transition_mask(10,11)==254);
    p_ptr->rage=1;assert(water_depth_transition_mask(10,11)==255);p_ptr->rage=0;
    g_labyrinth_view_active=true;assert(water_depth_transition_mask(10,11)==255);g_labyrinth_view_active=false;
    assert(Rand_state_export()==rng&&turn==turns);
    shore_map(FEAT_WATER,255);cave_feat[9][11]=FEAT_DEEP_WATER;cave_info[9][11]=0;
    Term->total_erase=true;prt_map();Term_fresh();
    cave_info[9][11]=CAVE_MARK|CAVE_SEEN;lite_spot(9,11);Term_fresh();
    SDL_Surface* changed=capture(130);force_map_redraw();Term_fresh();SDL_Surface* full=capture(131);
    assert(same_surface(changed,full));SDL_DestroySurface(changed);SDL_DestroySurface(full);
    cave_set_feat(9,11,FEAT_WATER);Term_fresh();changed=capture(132);
    force_map_redraw();Term_fresh();full=capture(133);assert(same_surface(changed,full));
    SDL_DestroySurface(changed);SDL_DestroySurface(full);
    puts("Native shoal-to-sea composition: all depth masks and mixed land masks, diagonals/bridges, hidden-depth gating and discovery/removal repaint: PASS");
}
static void wet_knowledge_tests(void) {
    for(int kind=0;kind<3;kind++) {
        int feat=wet_features[kind];shore_map(feat,255);
        if(feat!=FEAT_POISON) {
            for(int i=0;i<8;i++) cave_feat[10+shore_dy[i]][11+shore_dx[i]]=
                i%2?FEAT_WATER:FEAT_DEEP_WATER;
            assert(liquid_transition_mask(10,11,feat)==255);
            cave_feat[9][11]=FEAT_BRIDGE_DEEP_WATER_H;
            assert(liquid_transition_mask(10,11,feat)==255);
        } else {
            cave_feat[9][11]=FEAT_WATER;assert(liquid_transition_mask(10,11,feat)==254);
            cave_feat[9][11]=FEAT_BRIDGE_POISON_H;assert(liquid_transition_mask(10,11,feat)==255);
        }
        for(int i=0;i<8;i++) cave_info[10+shore_dy[i]][11+shore_dx[i]]=0;
        assert(liquid_transition_mask(10,11,feat)==0);
        SDL_Surface* hidden=shore_surface(NULL,0,0,true);
        for(int i=0;i<8;i++) cave_feat[10+shore_dy[i]][11+shore_dx[i]]=FEAT_FLOOR;
        SDL_Surface* absent=shore_surface(NULL,0,0,true);assert(same_surface(hidden,absent));
        SDL_DestroySurface(hidden);SDL_DestroySurface(absent);
        shore_map(feat,255);
        for(int i=0;i<8;i++) cave_info[10+shore_dy[i]][11+shore_dx[i]]=CAVE_MARK;
        p_ptr->rage=1;assert(liquid_transition_mask(10,11,feat)==0);p_ptr->rage=0;
        g_labyrinth_view_active=true;assert(liquid_transition_mask(10,11,feat)==0);
        g_labyrinth_view_active=false;
    }
    puts("Shallow/deep share connectivity through bridges; acid separate; unknown pixels and rage/labyrinth hide knowledge: PASS");
}
static void wet_bank_tests(void) {
    u64b rng=Rand_state_export();s32b turns=turn;
    for(int kind=0;kind<3;kind++) {
        int feat=wet_features[kind],inner_seen=0;
        int first=feat==FEAT_POISON?16:0;
        for(int y=4;y<24;y+=2)for(int x=4;x<24;x+=2) {
            floor_reset(13);int normal=floor_tile(y,x+3);
            cave_feat[y][x]=feat;
            int inner=floor_tile(y,x+1),outer=floor_tile(y,x+2);
            assert(inner>>8==36&&outer==normal);
            assert((inner&255)>=first&&(inner&255)<=first+6&&!(inner&1));
            inner_seen|=1<<(((inner&255)-first)/2);
            assert(floor_tile(y,x+3)==normal);
            for(int n=0;n<5;n++)assert(floor_tile(y,x+1)==inner&&floor_tile(y,x+2)==outer);
            cave_info[y][x]=0;assert(floor_tile(y,x+1)==normal&&floor_tile(y,x+2)==normal);
            cave_info[y][x]=CAVE_MARK;p_ptr->rage=1;
            assert(floor_tile(y,x+1)==normal&&floor_tile(y,x+2)==normal);p_ptr->rage=0;
            g_labyrinth_view_active=true;
            assert(floor_tile(y,x+1)==normal&&floor_tile(y,x+2)==normal);g_labyrinth_view_active=false;
        }
        assert(inner_seen==15);
    }
    assert(Rand_state_export()==rng&&turn==turns);
    puts("One parsed bank ring with all four silt/stone variants; radius2 retains original floor, stable without RNG or knowledge leaks: PASS");
}
static void wet_redraw_tests(void) {
    for(int kind=0;kind<3;kind++) {
        int feat=wet_features[kind];shore_map(feat,1);cave_info[9][11]=0;
        Term->total_erase=true;frame_tick=0;prt_map();Term_fresh();
        byte a,ta;char c,tc;map_info(10,11,&a,&c,&ta,&tc);
        SDL_Surface* before=capture(110);
        cave_info[9][11]=CAVE_MARK|CAVE_SEEN;lite_spot(9,11);Term_fresh();
        byte a2,ta2;char c2,tc2;map_info(10,11,&a2,&c2,&ta2,&tc2);
        assert(a==a2&&c==c2&&ta==ta2&&tc==tc2);
        SDL_Surface* changed=capture(111);assert(!same_surface(before,changed));
        force_map_redraw();Term_fresh();SDL_Surface* full=capture(112);assert(same_surface(changed,full));
        SDL_DestroySurface(before);SDL_DestroySurface(changed);SDL_DestroySurface(full);
        cave_set_feat(9,11,FEAT_FLOOR);Term_fresh();changed=capture(113);
        force_map_redraw();Term_fresh();full=capture(114);assert(same_surface(changed,full));
        SDL_DestroySurface(changed);SDL_DestroySurface(full);
        /* A lone source exercises bank invalidation without a second source masking it. */
        floor_reset(13);cave_feat[10][11]=feat;cave_info[10][11]=0;
        Term->total_erase=true;prt_map();Term_fresh();
        cave_info[10][11]=CAVE_MARK|CAVE_SEEN;lite_spot(10,11);Term_fresh();changed=capture(115);
        force_map_redraw();Term_fresh();full=capture(116);assert(same_surface(changed,full));
        SDL_DestroySurface(changed);SDL_DestroySurface(full);
        cave_set_feat(10,11,FEAT_FLOOR);Term_fresh();changed=capture(117);
        force_map_redraw();Term_fresh();full=capture(118);assert(same_surface(changed,full));
        SDL_DestroySurface(changed);SDL_DestroySurface(full);
    }
    puts("Same-glyph liquid connectivity and single bank reveal/remove repaint identically to full map: PASS");
}
static void wet_layer_tests(void) {
    free(r_info);r_info=calloc(8,sizeof(*r_info));mon_list=calloc(8,sizeof(*mon_list));
    o_list=calloc(8,sizeof(*o_list));k_info=calloc(8,sizeof(*k_info));mon_max=1;
    for(int kind=0;kind<3;kind++) {
        shore_map(wet_features[kind],255);frame_tick=0;Term->total_erase=true;prt_map();Term_fresh();
        SDL_Surface* bare=capture(120);
        k_info[3].x_attr=TILE_FLAG;k_info[3].x_char=(char)(TILE_FLAG|2);
        o_list[1].k_idx=3;o_list[1].marked=true;cave_o_idx[10][11]=1;
        lite_spot(10,11);Term_fresh();SDL_Surface* object=capture(121);assert(!same_surface(bare,object));
        SDL_DestroySurface(object);cave_o_idx[10][11]=0;
        r_info[1].x_attr=TILE_FLAG;r_info[1].x_char=(char)(TILE_FLAG|2);
        mon_list[1].r_idx=1;mon_list[1].fy=10;mon_list[1].fx=11;
        mon_list[1].ml=true;mon_list[1].alertness=ALERTNESS_ALERT;cave_m_idx[10][11]=1;
        lite_spot(10,11);Term_fresh();SDL_Surface* actor=capture(122);assert(!same_surface(bare,actor));
        SDL_DestroySurface(actor);cave_m_idx[10][11]=0;lite_spot(10,11);Term_fresh();
        SDL_Surface* restored=capture(123);assert(same_surface(bare,restored));
        SDL_DestroySurface(restored);
        int bridge=kind==0?FEAT_BRIDGE_WATER_H:kind==1?FEAT_BRIDGE_DEEP_WATER_H:FEAT_BRIDGE_POISON_H;
        f_info[bridge].x_char=(char)(TILE_FLAG|1);cave_feat[10][11]=bridge;
        lite_spot(10,11);Term_fresh();SDL_Surface* deck=capture(124);
        assert(!same_surface(bare,deck));force_map_redraw();Term_fresh();
        restored=capture(125);assert(same_surface(deck,restored));
        SDL_DestroySurface(deck);SDL_DestroySurface(restored);SDL_DestroySurface(bare);
        int seen_frames=0;Uint64 animated_now=frame_tick*IDLE_STEP_NS;
        for(int step=0;step<32;step++) {
            animated_now+=IDLE_STEP_NS;sdl_idle_animation_update(animated_now);
            for(int i=0;i<cell_count;i++)if(cells[i].y==10&&cells[i].x==11) {
                assert(cells[i].drawn_frame<(kind==2?3:4));seen_frames|=1<<cells[i].drawn_frame;
            }
        }
        assert(seen_frames==(kind==2?7:15));
    }
    int loads=image_loads,textures=texture_creations,mallocs=allocations;
    u64b rng=Rand_state_export();s32b turns=turn;Uint64 now=frame_tick*IDLE_STEP_NS;
    for(int i=0;i<2000;i++){now+=IDLE_STEP_NS;sdl_idle_animation_update(now);}
    assert(loads==image_loads&&textures==texture_creations&&mallocs==allocations);
    assert(Rand_state_export()==rng&&turn==turns);
    puts("Objects/actors remain above all new surfaces; cached idle frames make no loads/textures/allocations/RNG changes: PASS");
}
static void wet_fallback_tests(void) {
    sdl_idle_animation_shutdown();missing_transition=true;
    for(int kind=0;kind<3;kind++) {
        int feat=wet_features[kind];shore_map(feat,0);assert(load_liquid_texture(feat));
        SDL_Surface* actual=shore_surface(NULL,0,0,true);
        SDL_Surface* expected=wet_expected(feat==FEAT_POISON?poison_texture:water_texture,-1,0,255,feat==FEAT_DEEP_WATER);
        assert(same_surface(actual,expected));SDL_DestroySurface(actual);SDL_DestroySurface(expected);
        assert(!load_liquid_transition_texture(feat));int loads=image_loads;
        for(int n=0;n<5;n++){actual=shore_surface(NULL,0,n%3,true);SDL_DestroySurface(actual);}
        assert(image_loads==loads);
    }
    missing_transition=false;sdl_idle_animation_shutdown();
    puts("Missing river/deep/acid transition atlases retain original fallback and cache failure: PASS");
}
static void wet_preview(void) {
    const int w=26,h=19,scale=2;
    SDL_Texture* previous=SDL_GetRenderTarget(g_state.renderer);
    SDL_Texture* target=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,w*16*scale*2,h*16*scale);assert(target);
    SDL_SetRenderTarget(g_state.renderer,target);
    for(int panel=0;panel<2;panel++) {
        floor_reset(13);
        for(int y=0;y<h;y++)for(int x=0;x<w;x++) {
            bool wall=y==0||y==h-1||x==0||x==w-1;
            bool pool=y>=3&&y<=15&&x>=3&&x<=21&&!(y<=4&&x<=5)&&!(y>=13&&x>=18);
            bool river=false;
            bool shoal=y<=5||y>=14||x<=5||x>=20||(x>=9&&x<=11&&y>=8&&y<=10)
                ||(x==15&&y>=6&&y<=8)||(x==7&&y==12);
            int wet=panel?FEAT_POISON:shoal?FEAT_WATER:FEAT_DEEP_WATER;
            cave_feat[y][x]=wall?FEAT_WALL_EXTRA:(pool||river)?wet:FEAT_FLOOR;
            cave_info[y][x]=CAVE_MARK|CAVE_SEEN|CAVE_GLOW|(wall?CAVE_WALL:0);
        }
        for(int y=0;y<h;y++)for(int x=0;x<w;x++) {
            byte a,ta;char c,tc;map_info(y,x,&a,&c,&ta,&tc);
            SDL_FRect dst={(panel*w+x)*16*scale,y*16*scale,16*scale,16*scale};
            sdl_draw_map_tile_layers_at(y,x,a,c,ta,tc,&dst);
        }
    }
    for(int i=0;i<3;i++)assert(load_liquid_transition_texture(wet_features[i]));
    SDL_Surface* surface=SDL_RenderReadPixels(g_state.renderer,NULL);assert(surface);
    assert(IMG_SavePNG(surface,"scripts/output/river-acid-tiles-check/preview.png"));
    SDL_DestroySurface(surface);SDL_SetRenderTarget(g_state.renderer,previous);SDL_DestroyTexture(target);
}
'''


def main():
    idle.OUT = idle.ROOT / "scripts/output/river-acid-tiles-check"
    idle.HARNESS = idle.HARNESS.replace("scripts/output/idle-animation-check", "scripts/output/river-acid-tiles-check")
    idle.HARNESS = idle.HARNESS.replace("static bool missing_asset;", "static bool missing_asset, missing_transition;")
    idle.HARNESS = idle.HARNESS.replace("return missing_asset ? NULL : IMG_Load(path);",
        'return missing_asset || (missing_transition && strstr(path,"transition_")) ? NULL : IMG_Load(path);')
    idle.HARNESS = idle.HARNESS.replace("int main(void) {", floors.TESTS + shores.TESTS + TESTS + "\nint main(void) {")
    idle.HARNESS = idle.HARNESS.replace("    asynchronous_tests();",
        "    asynchronous_tests();\n    floor_templates();\n"
        "    f_info[FEAT_WATER].x_char=f_info[FEAT_DEEP_WATER].x_char=f_info[FEAT_POISON].x_char=(char)(TILE_FLAG|1);\n"
        "    wet_atlas_tests();\n    wet_depth_tests();\n    wet_knowledge_tests();\n    wet_bank_tests();\n    wet_redraw_tests();\n"
        "    wet_layer_tests();\n    wet_fallback_tests();\n    wet_preview();")
    idle.main()


if __name__ == "__main__":
    main()

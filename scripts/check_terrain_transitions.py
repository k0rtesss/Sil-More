#!/usr/bin/env python3
"""Production SDL checks for connected ice and lava terrain.

Build first. Uses isolated maps and source templates, never player saves.
Exercises atlas slots, visibility, fallback, redraws and animation caching.
"""
import check_idle_animation as idle
import check_cave_floor_tiles as floors

TESTS = r'''
static const int shore_dy[8]={-1,-1,0,1,1,1,0,-1};
static const int shore_dx[8]={0,1,1,1,0,-1,-1,-1};
/* A diagonal connects only when both of its adjacent cardinals connect. */
static int normalized_shore(int mask) {
    for(int i=1;i<8;i+=2)
        if(!(mask&(1<<(i-1))) || !(mask&(1<<((i+1)%8)))) mask&=~(1<<i);
    return mask;
}
static void shore_map(int feat,int mask) {
    floor_reset(feat==FEAT_ICE?62:63);
    cave_feat[10][11]=feat;
    for(int i=0;i<8;i++) cave_feat[10+shore_dy[i]][11+shore_dx[i]]=
        mask&(1<<i)?feat:FEAT_FLOOR;
}
static SDL_Surface* shore_surface(SDL_Texture* texture,int mask,int frame,bool production) {
    SDL_Texture* previous=SDL_GetRenderTarget(g_state.renderer);
    SDL_Texture* target=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,16,16); assert(target);
    SDL_SetRenderTarget(g_state.renderer,target);
    SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255); SDL_RenderClear(g_state.renderer);
    SDL_FRect dst={0,0,16,16};
    if(production) { frame_tick=frame*8; assert(draw_liquid(10,11,&dst)); }
    else {
        SDL_FRect src={mask<0?frame*16:(mask%16)*16,
            mask<0?0:(mask/16+frame*16)*16,16,16};
        SDL_SetTextureColorMod(texture,255,255,255);
        assert(SDL_RenderTexture(g_state.renderer,texture,&src,&dst));
    }
    SDL_Surface* result=SDL_RenderReadPixels(g_state.renderer,NULL); assert(result);
    SDL_SetRenderTarget(g_state.renderer,previous); SDL_DestroyTexture(target);
    return result;
}
static void shore_atlas_tests(void) {
    u64b rng=Rand_state_export(); s32b turns=turn;
    int seen[256]={0},count=0;
    for(int mask=0;mask<256;mask++) if(!seen[normalized_shore(mask)]++) count++;
    assert(count==47);
    for(int kind=0;kind<2;kind++) {
        int feat=kind?FEAT_LAVA:FEAT_ICE;
        assert(load_liquid_texture(feat));
        SDL_Texture* atlas=load_liquid_transition_texture(feat); assert(atlas);
        for(int mask=0;mask<256;mask++) {
            shore_map(feat,mask); assert(liquid_transition_mask(10,11,feat)==mask);
            for(int frame=0;frame<(kind?4:1);frame++) {
                SDL_Surface* actual=shore_surface(atlas,mask,frame,true);
                SDL_Surface* expected=shore_surface(atlas,normalized_shore(mask),frame,false);
                assert(same_surface(actual,expected));
                SDL_DestroySurface(actual); SDL_DestroySurface(expected);
            }
        }
        /* Full interiors preserve original source tile/frame pixels. */
        for(int frame=0;frame<(kind?4:1);frame++) {
            SDL_Surface* full=shore_surface(atlas,255,frame,false);
            SDL_Surface* original=shore_surface(kind?lava_texture:ice_texture,-1,frame,false);
            assert(same_surface(full,original));
            SDL_DestroySurface(full); SDL_DestroySurface(original);
        }
        shore_map(feat,0);
        SDL_Surface* isolated=shore_surface(atlas,0,0,true);
        shore_map(feat,255);
        SDL_Surface* interior=shore_surface(atlas,255,0,true);
        assert(!same_surface(isolated,interior));
        SDL_DestroySurface(isolated); SDL_DestroySurface(interior);
    }
    assert(Rand_state_export()==rng && turn==turns);
    puts("All 256 raw masks map to 47 connected shapes; actual SDL ice/lava frame selection and exact original full interiors: PASS");
}
static void shore_knowledge_tests(void) {
    for(int kind=0;kind<2;kind++) {
        int feat=kind?FEAT_LAVA:FEAT_ICE;
        shore_map(feat,255);
        for(int i=0;i<8;i++) cave_info[10+shore_dy[i]][11+shore_dx[i]]=0;
        assert(liquid_transition_mask(10,11,feat)==0);
        SDL_Surface* hidden=shore_surface(NULL,0,0,true);
        for(int i=0;i<8;i++) cave_feat[10+shore_dy[i]][11+shore_dx[i]]=FEAT_FLOOR;
        SDL_Surface* absent=shore_surface(NULL,0,0,true);
        assert(same_surface(hidden,absent));
        SDL_DestroySurface(hidden); SDL_DestroySurface(absent);
        shore_map(feat,255);
        for(int i=0;i<8;i++) cave_info[10+shore_dy[i]][11+shore_dx[i]]=CAVE_MARK;
        assert(liquid_transition_mask(10,11,feat)==255);
        p_ptr->rage=1; assert(liquid_transition_mask(10,11,feat)==0); p_ptr->rage=0;
        g_labyrinth_view_active=true; assert(liquid_transition_mask(10,11,feat)==0);
        g_labyrinth_view_active=false;
        cave_feat[9][11]=kind?FEAT_BRIDGE_LAVA_H:FEAT_BRIDGE_ICE_H;
        assert(liquid_transition_mask(10,11,feat)==255);
        cave_m_idx[9][11]=-1; assert(liquid_transition_mask(10,11,feat)==255);
        cave_feat[10][11]=kind?FEAT_BRIDGE_LAVA_V:FEAT_BRIDGE_ICE_V;
        assert(visible_liquid(10,11)==feat);
        cave_info[10][11]=0; assert(!visible_liquid(10,11));
        cave_info[10][11]=CAVE_MARK; p_ptr->rage=1; assert(!visible_liquid(10,11));
        p_ptr->rage=0; g_labyrinth_view_active=true; assert(!visible_liquid(10,11));
        g_labyrinth_view_active=false;
    }
    puts("Unknown neighbors have identical pixels; remembered/rage/labyrinth gating and bridge/actor connectivity: PASS");
}
static void shore_redraw_tests(void) {
    shore_map(FEAT_ICE,1); cave_info[9][11]=0;
    Term->total_erase=true; prt_map(); Term_fresh();
    SDL_Surface* before=capture(90);
    byte a,ta; char c,tc; map_info(10,11,&a,&c,&ta,&tc);
    cave_info[9][11]=CAVE_MARK|CAVE_SEEN; lite_spot(9,11); Term_fresh();
    byte a2,ta2; char c2,tc2; map_info(10,11,&a2,&c2,&ta2,&tc2);
    assert(a==a2 && c==c2 && ta==ta2 && tc==tc2);
    SDL_Surface* changed=capture(91); assert(!same_surface(before,changed));
    force_map_redraw(); Term_fresh(); SDL_Surface* full=capture(92);
    assert(same_surface(changed,full));
    SDL_DestroySurface(before); SDL_DestroySurface(changed); SDL_DestroySurface(full);
    cave_set_feat(9,11,FEAT_FLOOR); Term_fresh(); changed=capture(93);
    force_map_redraw(); Term_fresh(); full=capture(94);
    assert(same_surface(changed,full));
    SDL_DestroySurface(changed); SDL_DestroySurface(full);
    for(int big=0;big<2;big++) for(int pan=0;pan<4;pan++) {
        if(!pan) Term->total_erase=true;
        use_bigtile=big; p_ptr->wx=pan==3?0:pan;
        prt_map(); Term_fresh(); changed=capture(95);
        force_map_redraw(); Term_fresh(); full=capture(96);
        assert(same_surface(changed,full));
        SDL_DestroySurface(changed); SDL_DestroySurface(full);
    }
    shore_map(FEAT_LAVA,255); Term->total_erase=true; prt_map(); Term_fresh();
    int loads=image_loads,textures=texture_creations,mallocs=allocations;
    u64b rng=Rand_state_export(); s32b turns=turn;
    Uint64 now=frame_tick*IDLE_STEP_NS;
    for(int i=0;i<2000;i++) {now+=IDLE_STEP_NS;sdl_idle_animation_update(now);}
    assert(loads==image_loads && textures==texture_creations && mallocs==allocations);
    assert(Rand_state_export()==rng && turn==turns);
    puts("Same-glyph neighbor reveal/removal and pans match full repaint; 2000 idle steps without new loads/textures/allocations/RNG: PASS");
}
static void shore_fallback_tests(void) {
    sdl_idle_animation_shutdown(); missing_transition=true;
    for(int kind=0;kind<2;kind++) {
        int feat=kind?FEAT_LAVA:FEAT_ICE;
        shore_map(feat,0); assert(load_liquid_texture(feat));
        SDL_Surface* actual=shore_surface(NULL,0,0,true);
        SDL_Surface* expected=shore_surface(kind?lava_texture:ice_texture,-1,0,false);
        assert(same_surface(actual,expected));
        assert(kind?!lava_transition_texture:!ice_transition_texture);
        SDL_DestroySurface(actual); SDL_DestroySurface(expected);
        int loads=image_loads;
        for(int n=0;n<5;n++) {actual=shore_surface(NULL,0,0,true);SDL_DestroySurface(actual);}
        assert(image_loads==loads);
    }
    missing_transition=false; sdl_idle_animation_shutdown();
    puts("Missing transition atlases retain original terrain pixels, with failed-load caching: PASS");
}
static void material_transition_tests(void) {
    SDL_Texture* previous=SDL_GetRenderTarget(g_state.renderer);
    SDL_Texture* target=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,16,16); assert(target);
    SDL_SetRenderTarget(g_state.renderer,target);
    SDL_FRect dst={0,0,16,16};

    floor_reset(62);
    SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255); SDL_RenderClear(g_state.renderer);
    assert(!draw_elemental_transition(10,11,&dst));
    cave_color[10][12]=COLOR_STYLE_BASE+44;
    SDL_RenderClear(g_state.renderer);
    assert(draw_elemental_transition(10,11,&dst));
    assert(snow_dirt_transition_texture);
    SDL_Surface* snow=SDL_RenderReadPixels(g_state.renderer,NULL); assert(snow);

    /* A hidden dirt cell must not reveal a transition edge. */
    cave_info[10][12]=0;
    SDL_RenderClear(g_state.renderer);
    assert(!draw_elemental_transition(10,11,&dst));

    floor_reset(63);
    cave_color[10][12]=COLOR_STYLE_BASE+44;
    SDL_RenderClear(g_state.renderer);
    assert(draw_elemental_transition(10,11,&dst));
    assert(basalt_dirt_transition_texture);
    SDL_Surface* basalt=SDL_RenderReadPixels(g_state.renderer,NULL); assert(basalt);
    assert(!same_surface(snow,basalt));

    SDL_DestroySurface(snow); SDL_DestroySurface(basalt);
    SDL_SetRenderTarget(g_state.renderer,previous); SDL_DestroyTexture(target);

    /* Revealing a dirt neighbor must repaint the adjacent material even when
     * its terminal glyph remains the same. Otherwise the first visible frame
     * can retain a solid snow/basalt tile until a later full map repaint. */
    floor_reset(62);
    cave_color[10][12]=COLOR_STYLE_BASE+44;
    cave_info[10][12]=0;
    Term->total_erase=true; prt_map(); Term_fresh();
    SDL_Surface* before=capture(97); assert(before);
    cave_info[10][12]=CAVE_MARK|CAVE_SEEN;
    lite_spot(10,12); Term_fresh();
    SDL_Surface* incremental=capture(98); assert(incremental);
    force_map_redraw(); Term_fresh();
    SDL_Surface* full=capture(99); assert(full);
    assert(!same_surface(before,incremental));
    assert(same_surface(incremental,full));
    SDL_DestroySurface(before); SDL_DestroySurface(incremental);
    SDL_DestroySurface(full);

    puts("Snow-on-dirt and basalt-on-dirt use authored raw pixels; hidden dirt stays hidden: PASS");
}
static void material_bank_transition_tests(void) {
    SDL_Texture* previous=SDL_GetRenderTarget(g_state.renderer);
    SDL_Texture* target=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,16,16); assert(target);
    SDL_SetRenderTarget(g_state.renderer,target);
    SDL_FRect dst={0,0,16,16};
    u64b rng=Rand_state_export(); s32b turns=turn;
    for(int kind=0;kind<2;kind++) {
        int style=kind?63:62, hazard=kind?FEAT_LAVA:FEAT_ICE;
        /* Stored dirt beside a hazard is visibly snow/basalt. It must not
         * carve a dirt stripe into the adjacent material floor. */
        floor_reset(style);
        cave_color[10][12]=COLOR_STYLE_BASE+44;
        cave_feat[10][13]=hazard;
        assert((floor_tile(10,12)>>8)==35);
        assert((floor_tile(10,12)&255)==(kind?8:0));
        assert(elemental_transition_mask(10,11)==255);
        assert(!draw_elemental_transition(10,11,&dst));

        /* The converse: a bank with stored dirt needs a real transition
         * where the next floor is outside the hazard's bank radius. */
        floor_reset(style);
        cave_color[10][11]=cave_color[10][12]=COLOR_STYLE_BASE+44;
        cave_feat[10][10]=hazard;
        assert((floor_tile(10,11)>>8)==35);
        assert((floor_tile(10,11)&255)==(kind?8:0));
        assert((floor_tile(10,12)>>8)!=35);
        assert(elemental_transition_mask(10,11)==(255^4));
        assert(draw_elemental_transition(10,11,&dst));

        /* An actual dirt edge must not also carve edges toward unknown or
         * unlit remembered floors elsewhere around the same cell. */
        floor_reset(style);
        cave_color[10][12]=COLOR_STYLE_BASE+44;
        cave_info[9][11]=0;
        assert(elemental_transition_mask(10,11)==(255^4));
        cave_info[9][11]=CAVE_MARK; cave_light[9][11]=0;
        assert(elemental_transition_mask(10,11)==(255^4));
        cave_info[10][12]=CAVE_MARK; cave_light[10][12]=0;
        assert(elemental_transition_mask(10,11)==255);
        assert(!draw_elemental_transition(10,11,&dst));
        cave_light[10][12]=2;
        assert(elemental_transition_mask(10,11)==(255^4));
        assert(draw_elemental_transition(10,11,&dst));
        p_ptr->rage=1;
        assert(elemental_transition_mask(10,11)==255);
        assert(!draw_elemental_transition(10,11,&dst));
        p_ptr->rage=0; g_labyrinth_view_active=true;
        assert(elemental_transition_mask(10,11)==255);
        assert(!draw_elemental_transition(10,11,&dst));
        g_labyrinth_view_active=false;
        cave_info[10][12]=CAVE_MARK|CAVE_SEEN;
        cave_info[10][11]=CAVE_MARK; cave_light[10][11]=0;
        assert(!draw_elemental_transition(10,11,&dst));
        cave_light[10][11]=2;
        assert(draw_elemental_transition(10,11,&dst));

        /* A lone diagonal dirt cell creates a corner only across two visible
         * floor sides, never across a wall, hazard, or unknown side. Exercise
         * every rotation and either intervening side. */
        for(int diagonal=1;diagonal<8;diagonal+=2) {
            floor_reset(style);
            cave_color[10+shore_dy[diagonal]][11+shore_dx[diagonal]]=COLOR_STYLE_BASE+44;
            assert(elemental_transition_mask(10,11)==(255^(1<<diagonal)));
            assert(draw_elemental_transition(10,11,&dst));
            for(int side=0;side<2;side++) for(int blocker=0;blocker<4;blocker++) {
                floor_reset(style);
                cave_color[10+shore_dy[diagonal]][11+shore_dx[diagonal]]=COLOR_STYLE_BASE+44;
                int cardinal=(diagonal+(side?1:7))%8;
                int y=10+shore_dy[cardinal],x=11+shore_dx[cardinal];
                if(blocker==0) cave_feat[y][x]=FEAT_WALL_EXTRA;
                else if(blocker==1) cave_feat[y][x]=hazard;
                else if(blocker==2) cave_info[y][x]=0;
                else { cave_info[y][x]=CAVE_MARK; cave_light[y][x]=0; }
                assert(elemental_transition_mask(10,11)==255);
                assert(!draw_elemental_transition(10,11,&dst));
            }
        }
    }
    assert(Rand_state_export()==rng && turn==turns);
    SDL_SetRenderTarget(g_state.renderer,previous); SDL_DestroyTexture(target);
    puts("Rendered snow/basalt banks override stored dirt; genuine edges, dark donors and supported diagonal corners: PASS");
}
static void material_bank_preview(void) {
    const int w=18,h=14,scale=3;
    SDL_Texture* previous=SDL_GetRenderTarget(g_state.renderer);
    SDL_Texture* target=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,w*16*scale*2,h*16*scale); assert(target);
    SDL_SetRenderTarget(g_state.renderer,target);
    for(int panel=0;panel<2;panel++) {
        floor_reset(44);
        for(int y=0;y<h;y++) for(int x=0;x<w;x++) {
            if(x<7) cave_color[y][x]=COLOR_STYLE_BASE+(panel?63:62);
            if((x==7&&y>=2&&y<=10)||(y==7&&x>=7&&x<=12))
                cave_feat[y][x]=panel?FEAT_LAVA:FEAT_ICE;
            if(y==0||y==h-1||x==0||x==w-1||(y==4&&x>=11&&x<=14)) {
                cave_feat[y][x]=FEAT_WALL_EXTRA;
                cave_info[y][x]|=CAVE_WALL;
            }
        }
        for(int y=0;y<h;y++) for(int x=0;x<w;x++) {
            byte a,ta; char c,tc; map_info(y,x,&a,&c,&ta,&tc);
            SDL_FRect dst={(panel*w+x)*16*scale,y*16*scale,16*scale,16*scale};
            sdl_draw_map_tile_layers_at(y,x,a,c,ta,tc,&dst);
        }
    }
    SDL_Surface* surface=SDL_RenderReadPixels(g_state.renderer,NULL); assert(surface);
    assert(IMG_SavePNG(surface,"scripts/output/terrain-transitions-check/material-banks.png"));
    SDL_DestroySurface(surface); SDL_SetRenderTarget(g_state.renderer,previous);
    SDL_DestroyTexture(target);
}
static void material_bank_redraw_tests(void) {
    for(int kind=0;kind<2;kind++) {
        floor_reset(kind?63:62);
        cave_color[10][12]=COLOR_STYLE_BASE+44;
        cave_feat[10][13]=kind?FEAT_LAVA:FEAT_ICE;
        cave_info[10][13]=0;
        assert(elemental_transition_mask(10,11)==(255^4));
        Term->total_erase=true; prt_map(); Term_fresh();
        byte a,ta; char c,tc; map_info(10,11,&a,&c,&ta,&tc);
        SDL_Surface* before=capture(100+kind*3); assert(before);

        /* Revealing a hazard two cells away changes the intervening floor's
         * bank, hence this transition, although this cell's glyph is stable. */
        cave_info[10][13]=CAVE_MARK|CAVE_SEEN;
        lite_spot(10,13); Term_fresh();
        byte a2,ta2; char c2,tc2; map_info(10,11,&a2,&c2,&ta2,&tc2);
        assert(a==a2 && c==c2 && ta==ta2 && tc==tc2);
        assert(elemental_transition_mask(10,11)==255);
        SDL_Surface* changed=capture(101+kind*3); assert(changed);
        force_map_redraw(); Term_fresh();
        SDL_Surface* full=capture(102+kind*3); assert(full);
        assert(!same_surface(before,changed));
        assert(same_surface(changed,full));
        SDL_DestroySurface(before); SDL_DestroySurface(changed); SDL_DestroySurface(full);

        cave_set_feat(10,13,FEAT_FLOOR); Term_fresh();
        assert(elemental_transition_mask(10,11)==(255^4));
        changed=capture(106+kind*2); assert(changed);
        force_map_redraw(); Term_fresh();
        full=capture(107+kind*2); assert(full);
        assert(same_surface(changed,full));
        SDL_DestroySurface(changed); SDL_DestroySurface(full);
    }
    puts("Two-cell ice/lava bank reveal and removal repaint stable-glyph material transitions exactly like full redraw: PASS");
}
static void shore_preview(void) {
    const int w=24,h=18,scale=2;
    SDL_Texture* previous=SDL_GetRenderTarget(g_state.renderer);
    SDL_Texture* target=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,w*16*scale*2,h*16*scale); assert(target);
    SDL_SetRenderTarget(g_state.renderer,target);
    for(int panel=0;panel<2;panel++) {
        floor_reset(panel?63:62);
        for(int y=0;y<h;y++) for(int x=0;x<w;x++) {
            bool wall=y<1||y>=h-1||x<1||x>=w-1;
            bool pool=y>=3&&y<=11&&x>=3&&x<=11;
            if((y<=4&&x<=4)||(y>=9&&x>=9)||(y==6&&x<=6)) pool=false;
            bool river=x==17+(y/4)%2
                || (y>0&&y%4==0&&x==17+((y-1)/4)%2)
                || (y==7&&x>=15&&x<=20);
            bool isolated=(y==14&&x==4)||(y==14&&x==8);
            cave_feat[y][x]=wall?FEAT_WALL_EXTRA:(pool||river||isolated)?
                (panel?FEAT_LAVA:FEAT_ICE):FEAT_FLOOR;
            cave_info[y][x]=CAVE_MARK|CAVE_SEEN|CAVE_GLOW|(wall?CAVE_WALL:0);
        }
        for(int y=0;y<h;y++) for(int x=0;x<w;x++) {
            byte a,ta; char c,tc; map_info(y,x,&a,&c,&ta,&tc);
            SDL_FRect dst={(panel*w+x)*16*scale,y*16*scale,16*scale,16*scale};
            sdl_draw_map_tile_layers_at(y,x,a,c,ta,tc,&dst);
        }
    }
    assert(ice_transition_texture && lava_transition_texture);
    SDL_Surface* surface=SDL_RenderReadPixels(g_state.renderer,NULL); assert(surface);
    assert(IMG_SavePNG(surface,"scripts/output/terrain-transitions-check/preview.png"));
    SDL_DestroySurface(surface);SDL_SetRenderTarget(g_state.renderer,previous);SDL_DestroyTexture(target);
}
'''


def main():
    idle.OUT = idle.ROOT / "scripts/output/terrain-transitions-check"
    idle.HARNESS = idle.HARNESS.replace("scripts/output/idle-animation-check",
                                      "scripts/output/terrain-transitions-check")
    idle.HARNESS = idle.HARNESS.replace("static bool missing_asset;",
                                      "static bool missing_asset, missing_transition;")
    idle.HARNESS = idle.HARNESS.replace("return missing_asset ? NULL : IMG_Load(path);",
        'return missing_asset || (missing_transition && strstr(path,"transition_")) ? NULL : IMG_Load(path);')
    idle.HARNESS = idle.HARNESS.replace("int main(void) {", floors.TESTS + TESTS + "\nint main(void) {")
    idle.HARNESS = idle.HARNESS.replace("    asynchronous_tests();",
        "    asynchronous_tests();\n    floor_templates();\n    shore_atlas_tests();\n"
        "    shore_knowledge_tests();\n    shore_redraw_tests();\n    shore_fallback_tests();\n    material_transition_tests();\n    material_bank_transition_tests();\n    material_bank_redraw_tests();\n    shore_preview();\n    material_bank_preview();")
    idle.main()


if __name__ == "__main__":
    main()

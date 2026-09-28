#!/usr/bin/env python3
"""Production SDL checks for connected chasms with untouched adjacent artwork.

Build with build-incremental.ps1 first. Uses software SDL and isolated maps,
without opening player saves. Includes the existing idle regression harness.
"""
import check_idle_animation as idle
import check_cave_floor_tiles as floors

TESTS = r'''
static const int edge_dy[8]={-1,-1,0,1,1,1,0,-1};
static const int edge_dx[8]={0,1,1,1,0,-1,-1,-1};

static void edge_reset(void) {
    floor_reset(0);
    f_info[FEAT_CHASM].x_attr=TILE_FLAG|34;
    f_info[FEAT_CHASM].x_char=(char)TILE_FLAG;
}

static SDL_Surface* edge_cell(byte row,byte col,unsigned mask,byte feature) {
    edge_reset();
    cave_feat[10][11]=feature;
    for(int i=0;i<8;i++) {
        int y=10+edge_dy[i],x=11+edge_dx[i];
        cave_feat[y][x]=FEAT_CHASM;
        cave_info[y][x]=(mask&(1u<<i)) ? CAVE_MARK|CAVE_SEEN : 0;
    }
    SDL_Texture* previous=SDL_GetRenderTarget(g_state.renderer);
    SDL_Texture* target=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,16,16); assert(target);
    SDL_SetRenderTarget(g_state.renderer,target);
    SDL_FRect dst={0,0,16,16};
    sdl_draw_map_tile_layers_at(10,11,TILE_FLAG|row,(char)(TILE_FLAG|col),
        TILE_FLAG|row,(char)(TILE_FLAG|col),&dst);
    SDL_Surface* result=SDL_RenderReadPixels(g_state.renderer,NULL);assert(result);
    SDL_SetRenderTarget(g_state.renderer,previous);SDL_DestroyTexture(target);
    return result;
}

static void edge_art_tests(void) {
    const byte materials[][2]={{0,1},{0,4},{33,0},{33,2},{35,0},{35,8},{36,8},{36,16}};
    u64b rng=Rand_state_export();
    /* Compare every known-chasm neighborhood with the same unknown cells.
     * Neither floor nor wall pixels may acquire a lip, highlight or outline.
     * This directly exercises the production layer renderer, not a mask helper. */
    for(int kind=0;kind<2;kind++)for(unsigned i=0;i<N_ELEMENTS(materials);i++) {
        byte feat=kind?FEAT_WALL_EXTRA:FEAT_FLOOR;
        SDL_Surface* plain=edge_cell(materials[i][0],materials[i][1],0,feat);
        for(unsigned mask=1;mask<256;mask++) {
            SDL_Surface* actual=edge_cell(materials[i][0],materials[i][1],mask,feat);
            assert(same_surface(plain,actual));
            SDL_DestroySurface(actual);
        }
        SDL_DestroySurface(plain);
    }
    assert(Rand_state_export()==rng);
    puts("All 256 chasm neighborhoods leave eight original/imported floor and wall artworks unchanged: PASS");
    /* A fully opaque sprite must cover the edge underlay. */
    SDL_Texture* original=g_state.tileset;
    SDL_Surface* sheet=SDL_CreateSurface(48,16,SDL_PIXELFORMAT_RGBA32);assert(sheet);
    SDL_ClearSurface(sheet,0.7f,0.5f,0.3f,1.0f);
    SDL_Rect actor={32,0,16,16};
    SDL_FillSurfaceRect(sheet,&actor,SDL_MapSurfaceRGBA(sheet,20,200,60,255));
    g_state.tileset=SDL_CreateTextureFromSurface(g_state.renderer,sheet);assert(g_state.tileset);
    SDL_DestroySurface(sheet);
    edge_reset();cave_feat[10][12]=FEAT_CHASM;
    SDL_Texture* previous=SDL_GetRenderTarget(g_state.renderer);
    SDL_Texture* target=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,16,16);
    SDL_SetRenderTarget(g_state.renderer,target);SDL_FRect dst={0,0,16,16};
    sdl_draw_map_tile_layers_at(10,11,TILE_FLAG,(char)(TILE_FLAG|2),TILE_FLAG,(char)(TILE_FLAG|1),&dst);
    SDL_Surface* result=SDL_RenderReadPixels(g_state.renderer,NULL);assert(result);
    for(int y=0;y<16;y++)for(int x=0;x<16;x++) {
        Uint8 r,g,b,a;assert(SDL_ReadSurfacePixel(result,x,y,&r,&g,&b,&a));
        assert(r==20&&g==200&&b==60&&a==255);
    }
    SDL_DestroySurface(result);SDL_SetRenderTarget(g_state.renderer,previous);SDL_DestroyTexture(target);
    SDL_DestroyTexture(g_state.tileset);g_state.tileset=original;
    puts("Opaque sprites beside borderless chasms remain complete: PASS");
}

static void edge_redraw_tests(void) {
    edge_reset();cave_feat[10][12]=FEAT_CHASM;cave_info[10][12]=0;
    Term->total_erase=true;prt_map();Term_fresh();
    SDL_Surface* before=capture(201);
    cave_info[10][12]=CAVE_MARK|CAVE_SEEN;lite_spot(10,12);Term_fresh();
    SDL_Surface* incremental=capture(202);force_map_redraw();Term_fresh();
    SDL_Surface* full=capture(203);assert(!same_surface(before,incremental));assert(same_surface(incremental,full));
    SDL_DestroySurface(before);SDL_DestroySurface(incremental);SDL_DestroySurface(full);
    cave_set_feat(10,12,FEAT_FLOOR);Term_fresh();incremental=capture(204);
    force_map_redraw();Term_fresh();full=capture(205);assert(same_surface(incremental,full));
    SDL_DestroySurface(incremental);SDL_DestroySurface(full);
    cave_set_feat(10,12,FEAT_CHASM);Term_fresh();p_ptr->wx=1;prt_map();Term_fresh();incremental=capture(206);
    force_map_redraw();Term_fresh();full=capture(207);assert(same_surface(incremental,full));
    SDL_DestroySurface(incremental);SDL_DestroySurface(full);p_ptr->wx=0;
    prt_map();Term_fresh();
    /* The chasm glyph remains identical while its donor artwork changes. */
    cave_set_feat(10,11,FEAT_CHASM);cave_set_feat(10,12,FEAT_WALL_EXTRA);
    cave_color[10][12]=COLOR_STYLE_BASE+30;lite_spot(10,12);Term_fresh();
    before=capture(208);
    cave_color[10][12]=COLOR_STYLE_BASE+40;lite_spot(10,12);Term_fresh();
    incremental=capture(209);force_map_redraw();Term_fresh();full=capture(210);
    assert(!same_surface(before,incremental));assert(same_surface(incremental,full));
    SDL_DestroySurface(before);SDL_DestroySurface(incremental);SDL_DestroySurface(full);
    puts("Chasm discovery, removal, and pan incremental redraw match full repaint: PASS");
}

static SDL_Surface* contour_capture(void) {
    SDL_Texture* previous=SDL_GetRenderTarget(g_state.renderer);
    SDL_Texture* target=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,32,32);assert(target);
    SDL_SetRenderTarget(g_state.renderer,target);
    SDL_SetRenderDrawColor(g_state.renderer,251,0,197,255);SDL_RenderClear(g_state.renderer);
    SDL_FRect dst={8,8,16,16};
    sdl_draw_map_tile_layers_at(10,11,TILE_FLAG|34,(char)TILE_FLAG,
        TILE_FLAG|34,(char)TILE_FLAG,&dst);
    SDL_Surface* result=SDL_RenderReadPixels(g_state.renderer,NULL);assert(result);
    SDL_SetRenderTarget(g_state.renderer,previous);SDL_DestroyTexture(target);return result;
}

static Uint32 contour_pixel(SDL_Surface* s,int x,int y) {
    Uint8 r,g,b,a;assert(SDL_ReadSurfacePixel(s,x,y,&r,&g,&b,&a));
    return ((Uint32)r<<24)|((Uint32)g<<16)|((Uint32)b<<8)|a;
}

static void contour_reset(int style,byte feat,unsigned mask) {
    edge_reset();cave_feat[10][11]=FEAT_CHASM;
    for(int i=0;i<8;i++) {
        int y=10+edge_dy[i],x=11+edge_dx[i];
        cave_feat[y][x]=mask&(1u<<i)?feat:FEAT_CHASM;
        cave_color[y][x]=COLOR_STYLE_BASE+style;
    }
}

static SDL_Surface* contour_donor(int y,int x,bool floor) {
    byte a;char c;
    if(floor) map_info_floor_terrain(y,x,&a,&c);else map_info_terrain(y,x,&a,&c);
    SDL_Texture* previous=SDL_GetRenderTarget(g_state.renderer);
    SDL_Texture* target=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,16,16);assert(target);
    SDL_SetRenderTarget(g_state.renderer,target);
    SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255);SDL_RenderClear(g_state.renderer);
    SDL_FRect dst={0,0,16,16};sdl_draw_tileset_sprite(a,c,&dst,false);
    SDL_Surface* result=SDL_RenderReadPixels(g_state.renderer,NULL);assert(result);
    SDL_SetRenderTarget(g_state.renderer,previous);SDL_DestroyTexture(target);return result;
}

static void contour_tests(void) {
    const int styles[]={0,30,40,43};
    u64b rng=Rand_state_export();
    for(unsigned s=0;s<N_ELEMENTS(styles);s++)for(int wall=0;wall<2;wall++) {
        int changed=0;
        contour_reset(styles[s],FEAT_FLOOR,0);
        SDL_Surface* base=contour_capture();
        for(unsigned mask=1;mask<256;mask++) {
            contour_reset(styles[s],wall?FEAT_WALL_EXTRA:FEAT_FLOOR,mask);
            SDL_Surface* donor[8]={0};
            for(int i=0;i<8;i++)if(mask&(1u<<i))
                donor[i]=contour_donor(10+edge_dy[i],11+edge_dx[i],false);
            SDL_Surface* actual=contour_capture();
            for(int py=0;py<32;py++)for(int px=0;px<32;px++) {
                Uint32 pixel=contour_pixel(actual,px,py),original=contour_pixel(base,px,py);
                if(px<8||px>=24||py<8||py>=24||(px>=12&&px<20&&py>=12&&py<20))
                    assert(pixel==original); /* Outside destination and clear void center. */
                /* A one-cell winding crack stays open across every cardinal
                 * connection; rounding must not sever its center passage. */
                if((py==8&&px>=12&&px<20&&!(mask&1u))
                    ||(px==23&&py>=12&&py<20&&!(mask&(1u<<2)))
                    ||(py==23&&px>=12&&px<20&&!(mask&(1u<<4)))
                    ||(px==8&&py>=12&&py<20&&!(mask&(1u<<6)))) assert(pixel==original);
                if(pixel!=original) {
                    bool found=false;
                    for(int i=0;i<8;i++)if(donor[i])
                        found|=pixel==contour_pixel(donor[i],px-8,py-8);
                    assert(found); /* No synthetic highlights, shadows or outline colors. */
                    changed++;
                }
            }
            for(int i=0;i<8;i++)if(donor[i])SDL_DestroySurface(donor[i]);
            SDL_DestroySurface(actual);
        }
        assert(changed>0);SDL_DestroySurface(base);
    }
    assert(Rand_state_export()==rng);
    puts("All 256 masks: original/Verdant floor and wall donor pixels, intact central void, exact destination bounds, no synthetic border colors: PASS");

    contour_reset(40,FEAT_FLOOR,0);SDL_Surface* base=contour_capture();
    for(int mode=0;mode<4;mode++) {
        contour_reset(40,FEAT_WALL_EXTRA,1u<<2);
        cave_info[10][12]=mode?CAVE_MARK:0;
        if(mode==1)p_ptr->rage=1;
        if(mode==2)g_labyrinth_view_active=true;
        if(mode==3)p_ptr->image=1;
        SDL_Surface* actual=contour_capture();assert(same_surface(base,actual));SDL_DestroySurface(actual);
    }
    contour_reset(40,FEAT_FLOOR,0);cave_feat[10][12]=FEAT_BRIDGE_CHASM_H;
    SDL_Surface* bridge=contour_capture();assert(same_surface(base,bridge));SDL_DestroySurface(bridge);
    SDL_DestroySurface(base);
    /* Door and occupied donors use terrain; neither foreground sprite is sampled. */
    contour_reset(40,FEAT_FLOOR,1u<<2);base=contour_capture();
    cave_feat[10][12]=FEAT_DOOR_HEAD;
    SDL_Surface* door=contour_capture();assert(same_surface(base,door));SDL_DestroySurface(door);
    cave_feat[10][12]=FEAT_FLOOR;cave_m_idx[10][12]=1234;cave_o_idx[10][12]=1234;
    SDL_Surface* occupied=contour_capture();assert(same_surface(base,occupied));
    SDL_DestroySurface(occupied);SDL_DestroySurface(base);edge_reset();
    puts("Unknown/rage/labyrinth/hallucination donors hidden; chasm bridges connect; door/actor/object sprites never duplicated: PASS");

    /* A remembered donor uses its actual dark artwork. An unlit remembered
     * floor is invisible even though a remembered wall remains available. */
    contour_reset(40,FEAT_WALL_EXTRA,1u<<2);
    cave_info[10][12]=CAVE_MARK;cave_light[10][12]=0;
    SDL_Surface* dark=contour_donor(10,12,false),*actual=contour_capture();
    cave_info[10][12]=0;base=contour_capture();int changed=0;
    for(int py=0;py<16;py++)for(int px=0;px<16;px++) {
        Uint32 pixel=contour_pixel(actual,px+8,py+8);
        if(pixel!=contour_pixel(base,px+8,py+8)) {
            assert(pixel==contour_pixel(dark,px,py));changed++;
        }
    }
    assert(changed);SDL_DestroySurface(dark);SDL_DestroySurface(actual);
    cave_feat[10][12]=FEAT_FLOOR;cave_info[10][12]=CAVE_MARK;
    actual=contour_capture();assert(same_surface(actual,base));
    SDL_DestroySurface(actual);SDL_DestroySurface(base);
    puts("Remembered walls use dark source pixels; unlit remembered floor donors stay hidden: PASS");
}

static void contour_liquid_tests(void) {
    const byte feats[]={FEAT_WATER,FEAT_DEEP_WATER,FEAT_LAVA,FEAT_POISON,FEAT_ICE};
    for(unsigned i=0;i<N_ELEMENTS(feats);i++)for(int frame=0;frame<4;frame++) {
        contour_reset(40,FEAT_FLOOR,0);SDL_Surface* base=contour_capture();
        cave_feat[10][12]=feats[i];f_info[feats[i]].x_attr=TILE_FLAG;
        f_info[feats[i]].x_char=(char)(TILE_FLAG|1);
        /* Load on the first draw, then pin the shared clock for both samples. */
        SDL_Surface* warm=contour_capture();SDL_DestroySurface(warm);frame_tick=frame*8;
        SDL_Surface* actual=contour_capture();
        SDL_Texture* previous=SDL_GetRenderTarget(g_state.renderer);
        SDL_Texture* target=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
            SDL_TEXTUREACCESS_TARGET,16,16);assert(target);
        SDL_SetRenderTarget(g_state.renderer,target);
        SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255);SDL_RenderClear(g_state.renderer);
        SDL_FRect dst={0,0,16,16};assert(draw_liquid(10,12,&dst));
        SDL_Surface* donor=SDL_RenderReadPixels(g_state.renderer,NULL);assert(donor);
        SDL_SetRenderTarget(g_state.renderer,previous);SDL_DestroyTexture(target);
        int changed=0;
        for(int py=0;py<16;py++)for(int px=0;px<16;px++) {
            Uint32 pixel=contour_pixel(actual,px+8,py+8);
            if(pixel!=contour_pixel(base,px+8,py+8)) {
                assert(pixel==contour_pixel(donor,px,py));changed++;
            }
        }
        assert(changed);SDL_DestroySurface(base);SDL_DestroySurface(actual);SDL_DestroySurface(donor);
        if(feats[i]!=FEAT_ICE) {
            assert(chasm_has_animated_neighbor(10,11));
            assert(liquid_frame_count_at(10,11,FEAT_CHASM)==12);
        }
    }
    contour_reset(40,FEAT_LAVA,1u<<2);
    Term->total_erase=true;prt_map();Term_fresh();
    frame_tick=0;force_map_redraw();Term_fresh();
    SDL_Surface* before=capture(215);
    sdl_idle_animation_update(8*IDLE_STEP_NS);SDL_Surface* incremental=capture(216);
    force_map_redraw();Term_fresh();SDL_Surface* full=capture(217);
    assert(!same_surface(before,incremental));assert(same_surface(incremental,full));
    SDL_DestroySurface(before);SDL_DestroySurface(incremental);SDL_DestroySurface(full);
    int loads=image_loads,textures=texture_creations,mallocs=allocations;
    for(Uint64 tick=9;tick<1009;tick++)sdl_idle_animation_update(tick*IDLE_STEP_NS);
    assert(image_loads==loads&&texture_creations==textures&&allocations==mallocs);
    edge_reset();
    puts("Water/deep water/lava/acid/ice donors match production frames; incremental animation matches full repaint; 1,000 ticks with no new loads/textures/allocations: PASS");
}

static void edge_preview(void) {
    const byte mats[][2]={{0,1},{0,4},{33,0},{33,2},{35,0},{35,8},{36,8},{36,16}};
    const int w=10,h=8,scale=2;
    SDL_Texture* previous=SDL_GetRenderTarget(g_state.renderer);
    SDL_Texture* target=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,w*16*scale*4,h*16*scale*2);assert(target);
    SDL_SetRenderTarget(g_state.renderer,target);
    for(int panel=0;panel<8;panel++) {
        edge_reset();
        for(int y=0;y<h;y++)for(int x=0;x<w;x++) {
            bool hole=(x>=3&&x<=5&&y>=3&&y<=4);
            bool land=x>=1&&x<=8&&y>=1&&y<=6&&!hole;
            cave_feat[y][x]=land?(panel%2?FEAT_WALL_EXTRA:FEAT_FLOOR):FEAT_CHASM;
            cave_info[y][x]=CAVE_MARK|CAVE_SEEN|CAVE_GLOW;
        }
        for(int y=0;y<h;y++)for(int x=0;x<w;x++) {
            byte row=cave_feat[y][x]==FEAT_CHASM?34:mats[panel][0];
            byte col=cave_feat[y][x]==FEAT_CHASM?0:mats[panel][1];
            SDL_FRect dst={(panel%4*w+x)*16*scale,(panel/4*h+y)*16*scale,16*scale,16*scale};
            sdl_draw_map_tile_layers_at(y,x,TILE_FLAG|row,(char)(TILE_FLAG|col),TILE_FLAG|row,(char)(TILE_FLAG|col),&dst);
        }
    }
    SDL_Surface* image=SDL_RenderReadPixels(g_state.renderer,NULL);assert(image);
    assert(IMG_SavePNG(image,"scripts/output/chasm-edges-check/materials.png"));
    SDL_DestroySurface(image);SDL_SetRenderTarget(g_state.renderer,previous);SDL_DestroyTexture(target);
}

static void contour_preview(void) {
    const int w=18,h=14,scale=3,styles[]={30,40,0,43};
    SDL_Texture* previous=SDL_GetRenderTarget(g_state.renderer);
    SDL_Texture* target=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,w*16*scale*2,h*16*scale*2);assert(target);
    SDL_SetRenderTarget(g_state.renderer,target);
    for(int panel=0;panel<4;panel++) {
        edge_reset();
        for(int y=0;y<h;y++)for(int x=0;x<w;x++) {
            int center=7+(y/3)%3;
            int prior=7+((y-1)/3)%3;
            bool turn=y>2&&y%3==0&&x>=MIN(center,prior)&&x<=MAX(center,prior);
            bool crack=y>=2&&y<h-2&&(turn||(x>=center&&x<=center+(y>=5&&y<=8?3:0)));
            bool island=y==7&&x==center+2;
            cave_feat[y][x]=crack&&!island?FEAT_CHASM:
                (x<5||x>w-4||y<2||y>h-3?FEAT_WALL_EXTRA:FEAT_FLOOR);
            cave_info[y][x]=CAVE_MARK|CAVE_SEEN|CAVE_GLOW;
            cave_color[y][x]=COLOR_STYLE_BASE+styles[panel];
            cave_light[y][x]=2;
        }
        for(int y=0;y<h;y++)for(int x=0;x<w;x++) {
            byte a,ta;char c,tc;map_info(y,x,&a,&c,&ta,&tc);
            SDL_FRect dst={(panel%2*w+x)*16*scale,(panel/2*h+y)*16*scale,16*scale,16*scale};
            sdl_draw_map_tile_layers_at(y,x,a,c,ta,tc,&dst);
        }
    }
    SDL_Surface* result=SDL_RenderReadPixels(g_state.renderer,NULL);assert(result);
    assert(IMG_SavePNG(result,"scripts/output/chasm-edges-check/connected-chasms.png"));
    SDL_DestroySurface(result);SDL_SetRenderTarget(g_state.renderer,previous);SDL_DestroyTexture(target);
}
'''


def main():
    idle.OUT = idle.ROOT / "scripts/output/chasm-edges-check"
    idle.HARNESS = idle.HARNESS.replace("scripts/output/idle-animation-check", "scripts/output/chasm-edges-check")
    idle.HARNESS = idle.HARNESS.replace("int main(void) {", floors.TESTS + TESTS + "\nint main(void) {")
    idle.HARNESS = idle.HARNESS.replace("    asynchronous_tests();", "    asynchronous_tests();\n    floor_templates();\n    edge_art_tests();\n    contour_tests();\n    contour_liquid_tests();\n    edge_redraw_tests();\n    edge_preview();\n    contour_preview();")
    idle.main()


if __name__ == "__main__":
    main()

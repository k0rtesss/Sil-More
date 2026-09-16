#!/usr/bin/env python3
"""Production SDL chasm borders on arbitrary floor/wall artwork.

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

static SDL_Surface* edge_cell(byte row,byte col,bool chasm,byte feature) {
    edge_reset();
    cave_feat[10][11]=feature;
    if(chasm) cave_feat[10][12]=FEAT_CHASM;
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

static void edge_mask_tests(void) {
    edge_reset();u64b rng=Rand_state_export();
    assert(sdl_chasm_edge_mask(10,11)==0);
    for(int bit=0;bit<8;bit++) {
        edge_reset();int y=10+edge_dy[bit],x=11+edge_dx[bit];
        cave_feat[y][x]=FEAT_CHASM;
        assert(sdl_chasm_edge_mask(10,11)==(1u<<bit));
        cave_info[y][x]=0;assert(sdl_chasm_edge_mask(10,11)==0);
        cave_info[y][x]=CAVE_MARK;assert(sdl_chasm_edge_mask(10,11)==(1u<<bit));
        p_ptr->rage=1;assert(sdl_chasm_edge_mask(10,11)==0);p_ptr->rage=0;
        g_labyrinth_view_active=true;assert(sdl_chasm_edge_mask(10,11)==0);g_labyrinth_view_active=false;
        cave_info[y][x]=CAVE_MARK|CAVE_SEEN;
        cave_info[10][11]=0;assert(sdl_chasm_edge_mask(10,11)==0);
    }
    edge_reset();cave_feat[10][11]=FEAT_CHASM;cave_feat[10][12]=FEAT_CHASM;
    assert(sdl_chasm_edge_mask(10,11)==0);
    assert(sdl_chasm_edge_mask(-1,-1)==0);
    assert(sdl_chasm_edge_mask(p_ptr->cur_map_hgt,0)==0);
    assert(Rand_state_export()==rng);
    puts("Chasm adjacency: all directions, knowledge, rage/labyrinth and RNG: PASS");
}

static void edge_art_tests(void) {
    const byte materials[][2]={{0,1},{0,4},{33,0},{33,2},{35,0},{35,8},{36,8},{36,16}};
    for(int kind=0;kind<2;kind++)for(unsigned i=0;i<sizeof(materials)/sizeof(materials[0]);i++) {
        byte feat=kind?FEAT_WALL_EXTRA:FEAT_FLOOR;
        SDL_Surface* plain=edge_cell(materials[i][0],materials[i][1],false,feat);
        SDL_Surface* edged=edge_cell(materials[i][0],materials[i][1],true,feat);
        assert(!same_surface(plain,edged));
        Uint8 r1,g1,b1,a1,r2,g2,b2,a2;
        assert(SDL_ReadSurfacePixel(plain,8,8,&r1,&g1,&b1,&a1));
        assert(SDL_ReadSurfacePixel(edged,8,8,&r2,&g2,&b2,&a2));
        assert(r1==r2&&g1==g2&&b1==b2&&a1==a2);
        SDL_DestroySurface(plain);SDL_DestroySurface(edged);
    }
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
    puts("Arbitrary original/imported floors and walls edged; interiors and sprites preserved: PASS");
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
    puts("Chasm discovery, removal, and pan incremental redraw match full repaint: PASS");
}

static void edge_preview(void) {
    const byte mats[][2]={{0,1},{0,4},{33,0},{33,2},{35,0},{35,8},{36,8},{36,16}};
    const int w=10,h=8,scale=3;
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
'''


def main():
    idle.OUT = idle.ROOT / "scripts/output/chasm-edges-check"
    idle.HARNESS = idle.HARNESS.replace("scripts/output/idle-animation-check", "scripts/output/chasm-edges-check")
    idle.HARNESS = idle.HARNESS.replace("int main(void) {", floors.TESTS + TESTS + "\nint main(void) {")
    idle.HARNESS = idle.HARNESS.replace("    asynchronous_tests();", "    asynchronous_tests();\n    floor_templates();\n    edge_mask_tests();\n    edge_art_tests();\n    edge_redraw_tests();\n    edge_preview();")
    idle.main()


if __name__ == "__main__":
    main()

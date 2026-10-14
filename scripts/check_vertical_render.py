#!/usr/bin/env python3
"""Isolated production SDL aperture, fluid and foreground rendering checks.

The aperture reader is wrapped with a sanitized sample to test frontend
visibility independently. Real cache/LOS sampling belongs to the core check.
Uses the existing software SDL harness and never opens a player save.
"""
import check_idle_animation as idle
import check_cave_floor_tiles as floors

SAMPLE = r'''
#include "cave/cave-vertical.h"
static bool vertical_sample_enabled;
static bool vertical_preview_sample;
static cave_vertical_view_info vertical_sample;
static int vertical_reads;
bool __wrap_cave_vertical_view(int y, int x, cave_vertical_view_info* view) {
    vertical_reads++;
    if (!vertical_sample_enabled) return false;
    if (y == 10 && x == 11) { *view = vertical_sample; return true; }
    if (vertical_preview_sample && ABS(y-10)<=1 && ABS(x-11)<=1
        && cave_feat[y][x]==FEAT_CHASM) {
        *view=vertical_sample;view->flags=CAVE_VERTICAL_BELOW_VISIBLE;return true;
    }
    return false;
}
'''

TESTS = r'''
static void vertical_reset(void) {
    floor_reset(43);
    vertical_sample_enabled=false;
    vertical_preview_sample=false;
    vertical_sample=(cave_vertical_view_info){.flags=CAVE_VERTICAL_BELOW_VISIBLE,
        .feature=FEAT_FLOOR,.attr=TILE_FLAG|35,.chr=(char)(TILE_FLAG|2),
        .fluid_feature=FEAT_WATER,.direction=6,.relative_depth=1};
    f_info[FEAT_CHASM].x_attr=TILE_FLAG|34;
    f_info[FEAT_CHASM].x_char=(char)TILE_FLAG;
    f_info[FEAT_BRIDGE_CHASM_H].x_char=(char)(TILE_FLAG|1);
    f_info[FEAT_WATER].x_char=f_info[FEAT_DEEP_WATER].x_char=(char)(TILE_FLAG|1);
    cave_feat[10][11]=FEAT_CHASM;
    for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++)
        cave_feat[10+dy][11+dx]=FEAT_CHASM;
}
static SDL_Surface* vertical_cell(void) {
    SDL_Texture* previous=SDL_GetRenderTarget(g_state.renderer);
    SDL_Texture* target=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,32,32);assert(target);
    SDL_SetRenderTarget(g_state.renderer,target);
    SDL_SetRenderDrawColor(g_state.renderer,251,0,197,255);SDL_RenderClear(g_state.renderer);
    byte a,ta;char c,tc;map_info(10,11,&a,&c,&ta,&tc);
    SDL_FRect dst={8,8,16,16};sdl_draw_map_tile_layers_at(10,11,a,c,ta,tc,&dst);
    SDL_Surface* image=SDL_RenderReadPixels(g_state.renderer,NULL);assert(image);
    SDL_SetRenderTarget(g_state.renderer,previous);SDL_DestroyTexture(target);return image;
}
static Uint32 vertical_pixel(SDL_Surface* surface,int x,int y) {
    Uint8 r,g,b,a;assert(SDL_ReadSurfacePixel(surface,x,y,&r,&g,&b,&a));
    return ((Uint32)r<<24)|((Uint32)g<<16)|((Uint32)b<<8)|a;
}
static void vertical_render_tests(void) {
    vertical_reset();u64b rng=Rand_state_export();s32b turns=turn;
    SDL_Surface* fallback=vertical_cell();vertical_sample_enabled=true;
    SDL_Surface* lower=vertical_cell();assert(!same_surface(fallback,lower));
    /* A lower surface occupies the normal 16px tile, without touching banks. */
    for(int y=0;y<32;y++)for(int x=0;x<32;x++)
        if(x<8||x>=24||y<8||y>=24)
            assert(vertical_pixel(lower,x,y)==vertical_pixel(fallback,x,y));
    for(int mode=0;mode<5;mode++) {
        vertical_reset();vertical_sample_enabled=true;
        if(mode==0)cave_info[10][11]=CAVE_MARK;
        if(mode==1)p_ptr->blind=1;
        if(mode==2)p_ptr->image=1;
        if(mode==3){p_ptr->rage=1;cave_info[10][11]=CAVE_MARK;}
        if(mode==4){g_labyrinth_view_active=true;cave_info[10][11]=CAVE_MARK;}
        SDL_Surface* hidden=vertical_cell();vertical_sample_enabled=false;
        SDL_Surface* reference=vertical_cell();assert(same_surface(hidden,reference));
        SDL_DestroySurface(hidden);SDL_DestroySurface(reference);
    }
    vertical_reset();vertical_sample_enabled=true;
    SDL_SetTextureColorMod(g_state.tileset,201,202,203);
    SDL_SetTextureAlphaMod(g_state.tileset,204);
    SDL_Surface* state=vertical_cell();SDL_DestroySurface(state);
    Uint8 r,g,b,a;SDL_GetTextureColorMod(g_state.tileset,&r,&g,&b);
    SDL_GetTextureAlphaMod(g_state.tileset,&a);assert(r==201&&g==202&&b==203&&a==204);
    SDL_SetTextureColorMod(g_state.tileset,255,255,255);SDL_SetTextureAlphaMod(g_state.tileset,255);
    for(int direction=2;direction<=8;direction+=2) {
        vertical_reset();vertical_sample_enabled=true;vertical_sample.flags|=CAVE_VERTICAL_OUTFLOW;
        vertical_sample.direction=direction;
        SDL_Surface* flowing=vertical_cell();assert(!same_surface(lower,flowing));
        SDL_DestroySurface(flowing);
    }
    vertical_reset();vertical_sample_enabled=true;cave_feat[10][11]=FEAT_BRIDGE_CHASM_H;
    SDL_Surface* bridge=vertical_cell();vertical_sample_enabled=false;
    SDL_Surface* bridge_ref=vertical_cell();
    assert(!same_surface(bridge,bridge_ref));
    for(int y=11;y<21;y++)for(int x=8;x<24;x++)
        assert(vertical_pixel(bridge,x,y)==vertical_pixel(bridge_ref,x,y));
    SDL_DestroySurface(bridge);SDL_DestroySurface(bridge_ref);
    vertical_reset();vertical_sample_enabled=true;cave_feat[10][11]=FEAT_FLOOR;
    SDL_Surface* dry=vertical_cell();vertical_sample.flags=CAVE_VERTICAL_INFLOW;
    SDL_Surface* incoming=vertical_cell();assert(!same_surface(dry,incoming));
    vertical_sample.fluid_feature=0;
    SDL_Surface* stopped=vertical_cell();assert(same_surface(dry,stopped));
    SDL_DestroySurface(stopped);vertical_sample.fluid_feature=FEAT_WATER;
    int col=COL_MAP+11,row=ROW_MAP+10;byte at,ta;char ch,tc;
    map_info(10,11,&at,&ch,&ta,&tc);
    sdl_idle_animation_track(col,row,10,11,at,ch,ta,tc);
    assert(cell_count==1&&cells[0].vertical_effect&&!cells[0].liquid_feat);
    sdl_idle_animation_clear_cells();
    SDL_DestroySurface(dry);SDL_DestroySurface(incoming);
    assert(Rand_state_export()==rng&&turn==turns);
    SDL_DestroySurface(fallback);SDL_DestroySurface(lower);
    puts("Aperture terrain, fog/blind/image/rage/labyrinth gates, unchanged cell bounds, bridge foreground, four flow directions, ceiling inflow, animation tracking and shared state: PASS");
}
static void vertical_preview(void) {
    const int scale=3,w=9,h=7,panels=8,columns=4;
    SDL_Texture* previous=SDL_GetRenderTarget(g_state.renderer);
    SDL_Texture* target=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,w*16*scale*columns,h*16*scale*2);assert(target);
    SDL_SetRenderTarget(g_state.renderer,target);
    for(int panel=0;panel<panels;panel++) {
        vertical_reset();vertical_sample_enabled=true;vertical_preview_sample=true;
        vertical_sample.flags|=panel==3?CAVE_VERTICAL_INFLOW:CAVE_VERTICAL_OUTFLOW;
        vertical_sample.fluid_feature=panel==1?FEAT_LAVA:FEAT_WATER;
        cave_feat[10][10]=panel==1?FEAT_LAVA:FEAT_WATER;
        if(panel==2)cave_feat[10][11]=FEAT_BRIDGE_CHASM_H;
        if(panel==3)cave_feat[10][11]=FEAT_WATER;
        if(panel>=4) {
            vertical_sample.flags=panel==4?CAVE_VERTICAL_CEILING|CAVE_VERTICAL_INFLOW
                :CAVE_VERTICAL_PASSAGE|CAVE_VERTICAL_OUTFLOW;
            if(panel==6)vertical_sample.flags|=CAVE_VERTICAL_DIVE;
            if(panel==7)vertical_sample.flags|=CAVE_VERTICAL_DRY_TUBE;
            vertical_sample.fluid_feature=0;vertical_preview_sample=false;
            for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++)
                cave_feat[10+dy][11+dx]=panel==5?FEAT_WATER
                    :panel==6?FEAT_DEEP_WATER:FEAT_FLOOR;
        }
        for(int y=7;y<7+h;y++)for(int x=7;x<7+w;x++) {
            byte a,ta;char c,tc;map_info(y,x,&a,&c,&ta,&tc);
            SDL_FRect dst={(panel%columns*w+x-7)*16*scale,
                (panel/columns*h+y-7)*16*scale,16*scale,16*scale};
            sdl_draw_map_tile_layers_at(y,x,a,c,ta,tc,&dst);
        }
    }
    SDL_Surface* image=SDL_RenderReadPixels(g_state.renderer,NULL);assert(image);
    assert(IMG_SavePNG(image,"scripts/output/vertical-render-check/connected-floors.png"));
    SDL_DestroySurface(image);SDL_SetRenderTarget(g_state.renderer,previous);SDL_DestroyTexture(target);
    vertical_reset();
}
static void vertical_entrance_tests(void) {
    for(int kind=0;kind<5;kind++) {
    vertical_reset();
    int feature=kind==1||kind==4?FEAT_WATER:kind==2?FEAT_DEEP_WATER:FEAT_FLOOR;
    cave_feat[10][11]=feature;
    SDL_Surface* plain=vertical_cell();vertical_sample_enabled=true;
    u64b rng=Rand_state_export();s32b clock_before=turn;Uint64 tick_before=frame_tick;
    byte flags=kind==0?CAVE_VERTICAL_CEILING|CAVE_VERTICAL_INFLOW
        :CAVE_VERTICAL_PASSAGE|(kind==4?CAVE_VERTICAL_INFLOW:CAVE_VERTICAL_OUTFLOW);
    if(kind==2)flags|=CAVE_VERTICAL_DIVE;
    if(kind==3)flags|=CAVE_VERTICAL_DRY_TUBE;
    vertical_sample.flags=flags;
    vertical_sample.fluid_feature=0;
    SDL_Surface* ceiling=vertical_cell();assert(!same_surface(plain,ceiling));
    if(kind==4) {
        vertical_sample.flags=CAVE_VERTICAL_PASSAGE|CAVE_VERTICAL_OUTFLOW;
        SDL_Surface* outgoing=vertical_cell();assert(!same_surface(outgoing,ceiling));
        SDL_DestroySurface(outgoing);vertical_sample.flags=flags;
    }
    for(int y=0;y<32;y++)for(int x=0;x<32;x++)
        if(x<8||x>=24||y<8||y>=24)
            assert(vertical_pixel(plain,x,y)==vertical_pixel(ceiling,x,y));
    frame_tick+=8;SDL_Surface* dust=vertical_cell();assert(!same_surface(ceiling,dust));
    SDL_DestroySurface(dust);frame_tick=tick_before;
    for(int mode=0;mode<2;mode++) {
        if(mode==0)cave_info[10][11]=CAVE_MARK;else p_ptr->blind=1;
        SDL_Surface* hidden=vertical_cell();vertical_sample_enabled=false;
        SDL_Surface* reference=vertical_cell();assert(same_surface(hidden,reference));
        SDL_DestroySurface(hidden);SDL_DestroySurface(reference);
        cave_info[10][11]=CAVE_MARK|CAVE_SEEN;p_ptr->blind=0;vertical_sample_enabled=true;
    }
    SDL_BlendMode old_blend,blend;Uint8 old_r,old_g,old_b,old_a,r,g,b,a;
    SDL_GetRenderDrawBlendMode(g_state.renderer,&old_blend);
    SDL_GetRenderDrawColor(g_state.renderer,&old_r,&old_g,&old_b,&old_a);
    SDL_SetRenderDrawBlendMode(g_state.renderer,SDL_BLENDMODE_ADD);
    SDL_SetRenderDrawColor(g_state.renderer,17,29,41,53);
    SDL_FRect dst={8,8,16,16};assert(sdl_vertical_draw_effects(10,11,&dst));
    SDL_GetRenderDrawBlendMode(g_state.renderer,&blend);
    SDL_GetRenderDrawColor(g_state.renderer,&r,&g,&b,&a);
    assert(blend==SDL_BLENDMODE_ADD&&r==17&&g==29&&b==41&&a==53);
    SDL_SetRenderDrawBlendMode(g_state.renderer,old_blend);
    SDL_SetRenderDrawColor(g_state.renderer,old_r,old_g,old_b,old_a);
    byte at,ta;char ch,tc;map_info(10,11,&at,&ch,&ta,&tc);
    sdl_idle_animation_track(COL_MAP+11,ROW_MAP+10,10,11,at,ch,ta,tc);
    assert(cell_count==1&&cells[0].vertical_effect);
    assert(cells[0].liquid_feat==(feature==FEAT_FLOOR?0:feature));
    sdl_idle_animation_clear_cells();
    SDL_Texture* atlas=g_state.tileset;
    SDL_Surface* sheet=SDL_CreateSurface(48,16,SDL_PIXELFORMAT_RGBA32);assert(sheet);
    SDL_ClearSurface(sheet,0.3f,0.2f,0.1f,1.0f);
    SDL_Rect actor={32,0,16,16};
    SDL_FillSurfaceRect(sheet,&actor,SDL_MapSurfaceRGBA(sheet,20,200,60,255));
    g_state.tileset=SDL_CreateTextureFromSurface(g_state.renderer,sheet);assert(g_state.tileset);
    SDL_DestroySurface(sheet);
    SDL_Texture* previous=SDL_GetRenderTarget(g_state.renderer);
    SDL_Texture* target=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,16,16);assert(target);
    SDL_SetRenderTarget(g_state.renderer,target);SDL_FRect actor_dst={0,0,16,16};
    sdl_draw_map_tile_layers_at(10,11,TILE_FLAG,(char)(TILE_FLAG|2),
        TILE_FLAG,(char)(TILE_FLAG|1),&actor_dst);
    SDL_Surface* foreground=SDL_RenderReadPixels(g_state.renderer,NULL);assert(foreground);
    for(int y=0;y<16;y++)for(int x=0;x<16;x++)
        assert(vertical_pixel(foreground,x,y)==0x14c83cffU);
    SDL_DestroySurface(foreground);SDL_SetRenderTarget(g_state.renderer,previous);
    SDL_DestroyTexture(target);SDL_DestroyTexture(g_state.tileset);g_state.tileset=atlas;
    vertical_sample.flags=CAVE_VERTICAL_INFLOW;
    SDL_Surface* tunnel=vertical_cell();assert(same_surface(plain,tunnel));
    SDL_DestroySurface(tunnel);
    assert(Rand_state_export()==rng&&turn==clock_before&&frame_tick==tick_before);
    SDL_DestroySurface(plain);SDL_DestroySurface(ceiling);
    }
    puts("Dry ceiling, stream, dive, cooled tube and incoming reverse-arrow cues: normal-size animation, bounds, hidden/blind, actor foreground, renderer/RNG/clock and idle tracking PASS; unflagged dry passages unchanged");
}
static void vertical_redraw_tests(void) {
    vertical_reset();Term->total_erase=true;prt_map();Term_fresh();
    vertical_sample_enabled=true;lite_spot(10,11);Term_fresh();
    SDL_Surface* incremental=capture(301);force_map_redraw();Term_fresh();
    SDL_Surface* full=capture(302);assert(same_surface(incremental,full));
    SDL_DestroySurface(incremental);SDL_DestroySurface(full);
    for(int cue=0;cue<6;cue++) {
    vertical_reset();cave_feat[10][11]=FEAT_FLOOR;
    force_map_redraw();prt_map();Term_fresh();
    SDL_Surface* dry=capture(303);vertical_sample_enabled=true;
    vertical_sample.flags=CAVE_VERTICAL_INFLOW;
    if(cue>0) {
        vertical_sample.flags=cue==1?CAVE_VERTICAL_CEILING|CAVE_VERTICAL_INFLOW
            :CAVE_VERTICAL_PASSAGE|(cue==5?CAVE_VERTICAL_INFLOW:CAVE_VERTICAL_OUTFLOW);
        if(cue==3)vertical_sample.flags|=CAVE_VERTICAL_DIVE;
        if(cue==4)vertical_sample.flags|=CAVE_VERTICAL_DRY_TUBE;
        vertical_sample.fluid_feature=0;
    }
    lite_spot(10,11);Term_fresh();
    incremental=capture(304);assert(!same_surface(dry,incremental));
    force_map_redraw();Term_fresh();full=capture(305);assert(same_surface(incremental,full));
    SDL_DestroySurface(dry);SDL_DestroySurface(incremental);SDL_DestroySurface(full);
    vertical_sample_enabled=false;lite_spot(10,11);Term_fresh();
    incremental=capture(306);force_map_redraw();Term_fresh();full=capture(307);
    assert(same_surface(incremental,full));
    SDL_DestroySurface(incremental);SDL_DestroySurface(full);
    }
    puts("Lower sample arrival and wet/dry ceiling/stream/dive/tube/reverse cue activation/removal on unchanged floor match a full repaint: PASS");
}
'''

def main():
    idle.OUT = idle.ROOT / "scripts/output/vertical-render-check"
    idle.HARNESS = idle.HARNESS.replace("scripts/output/idle-animation-check", "scripts/output/vertical-render-check")
    idle.HARNESS = idle.HARNESS.replace('#include "sdl/render/sdl-idle-animation.c"', SAMPLE + '\n#include "sdl/render/sdl-idle-animation.c"')
    idle.HARNESS = idle.HARNESS.replace("int main(void) {", floors.TESTS + TESTS + "\nint main(void) {")
    idle.HARNESS = idle.HARNESS.replace("    asynchronous_tests();", "    asynchronous_tests();\n    floor_templates();\n    vertical_render_tests();\n    vertical_entrance_tests();\n    vertical_preview();\n    vertical_redraw_tests();")
    idle.EXTRA_SOURCES = [str(idle.ROOT / source) for source in
                          ("src/sdl/render/sdl-term-callbacks.c", "src/cave/cave-map-ui.c")]
    idle.EXCLUDE_OBJECTS = ("/src/sdl/render/sdl-term-callbacks.c.obj", "/src/cave/cave-map-ui.c.obj")
    idle.WRAP_SYMBOLS = ("cave_vertical_view",)
    idle.main()

if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Check connected melting-ice frames, idle updates and replacement pixels.

Build first with build-incremental.ps1. Uses production SDL rendering on an
isolated software map, without loading saves or changing user configuration.
Requires Pillow for source-border checks and the rendered GIF preview.
"""
from pathlib import Path
import check_idle_animation as idle
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "scripts/output/melting-ice-render-check"

TESTS = r'''
#include "init.h"
static void melt_expect_repaint(void) {
    Term_fresh(); SDL_Surface* incremental=capture(100);
    force_map_redraw(); Term_fresh(); SDL_Surface* full=capture(101);
    assert(same_surface(incremental,full));
    SDL_DestroySurface(incremental); SDL_DestroySurface(full);
}

static void melt_expect_source(SDL_Surface* canvas, int frame) {
    SDL_Surface* source=IMG_Load("lib/xtra/graf/transition_melting_ice_on_snow.png");
    assert(source && source->w==256 && source->h==1024);
    SDL_Surface* expected=SDL_ConvertSurface(source,canvas->format); assert(expected);
    int left=(COL_MAP+11)*16,top=(ROW_MAP+10)*16;
    for(int row=0;row<16;row++)
        assert(memcmp((byte*)canvas->pixels+(top+row)*canvas->pitch+left*4,
            (byte*)expected->pixels+(frame*256+row)*expected->pitch,16*4)==0);
    SDL_DestroySurface(source); SDL_DestroySurface(expected);
}

static const int melt_dy[8]={-1,-1,0,1,1,1,0,-1};
static const int melt_dx[8]={0,1,1,1,0,-1,-1,-1};

static void melt_neighbors(int center,int mask,int phase) {
    cave_feat[10][11]=center;cave_info[10][11]=CAVE_MARK|CAVE_SEEN;
    for(int i=0;i<8;i++) {
        int y=10+melt_dy[i],x=11+melt_dx[i];
        cave_feat[y][x]=(mask&(1<<i))
            ? ((i+phase)%2?FEAT_ICE:FEAT_MELTING_ICE):FEAT_FLOOR;
        cave_info[y][x]=CAVE_MARK|CAVE_SEEN;
    }
}

static SDL_Surface* melt_sample(int frame,SDL_Texture* reference,int mask) {
    SDL_Texture* previous=SDL_GetRenderTarget(g_state.renderer);
    SDL_Texture* target=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,16,16);assert(target);
    SDL_SetRenderTarget(g_state.renderer,target);
    SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255);SDL_RenderClear(g_state.renderer);
    SDL_FRect dst={0,0,16,16};frame_tick=frame*8;
    if(reference) {
        SDL_FRect src={(mask%16)*16,(mask/16+frame*16)*16,16,16};
        SDL_SetTextureColorMod(reference,255,255,255);
        assert(SDL_RenderTexture(g_state.renderer,reference,&src,&dst));
    } else assert(draw_liquid(10,11,&dst));
    SDL_Surface* result=SDL_RenderReadPixels(g_state.renderer,NULL);assert(result);
    SDL_SetRenderTarget(g_state.renderer,previous);SDL_DestroyTexture(target);
    return result;
}

static void melt_connection_tests(void) {
    u64b rng=Rand_state_export();s32b turns=turn;
    f_info[FEAT_ICE].x_char=(char)(TILE_FLAG|1);
    for(int kind=0;kind<2;kind++) {
        int center=kind?FEAT_MELTING_ICE:FEAT_ICE;
        SDL_Texture* atlas=load_liquid_transition_texture(center);assert(atlas);
        for(int phase=0;phase<2;phase++)for(int mask=0;mask<256;mask++) {
            melt_neighbors(center,mask,phase);
            assert(liquid_transition_mask(10,11,center)==mask);
            for(int frame=0;frame<(kind?4:1);frame++) {
                SDL_Surface* actual=melt_sample(frame,NULL,0);
                SDL_Surface* expected=melt_sample(frame,atlas,mask);
                assert(same_surface(actual,expected));
                SDL_DestroySurface(actual);SDL_DestroySurface(expected);
            }
        }
        melt_neighbors(center,255,0);
        for(int i=0;i<8;i++)cave_info[10+melt_dy[i]][11+melt_dx[i]]=0;
        assert(liquid_transition_mask(10,11,center)==0);
        SDL_Surface* hidden=melt_sample(0,NULL,0);
        melt_neighbors(center,0,0);
        SDL_Surface* absent=melt_sample(0,NULL,0);
        assert(same_surface(hidden,absent));
        SDL_DestroySurface(hidden);SDL_DestroySurface(absent);
        melt_neighbors(center,255,1);
        for(int i=0;i<8;i++)cave_info[10+melt_dy[i]][11+melt_dx[i]]=CAVE_MARK;
        assert(liquid_transition_mask(10,11,center)==255);
        p_ptr->rage=true;assert(liquid_transition_mask(10,11,center)==0);p_ptr->rage=false;
        g_labyrinth_view_active=true;assert(liquid_transition_mask(10,11,center)==0);
        g_labyrinth_view_active=false;
    }
    assert(Rand_state_export()==rng && turn==turns);
    puts("Mixed solid/melting ice: all 256 eight-neighbor masks, both alternating arrangements and both center types select exact atlas pixels; hidden/remembered neighbors respect visibility: PASS");

    for(int kind=0;kind<2;kind++) {
        int center=kind?FEAT_MELTING_ICE:FEAT_ICE;
        melt_neighbors(center,4,kind);
        cave_info[10][12]=0;frame_tick=0;
        Term->total_erase=true;prt_map();Term_fresh();
        cave_info[10][12]=CAVE_MARK|CAVE_SEEN;lite_spot(10,12);melt_expect_repaint();
        const int replacements[]={FEAT_ICE,FEAT_MELTING_ICE,FEAT_WATER,FEAT_FLOOR};
        for(int i=0;i<4;i++) {
            cave_set_feat(10,12,replacements[i]);melt_expect_repaint();
            assert(liquid_transition_mask(10,11,center)==(i<3?4:0));
        }
    }
    puts("Mixed ice neighbor discovery and solid/melting/water/floor replacement repaint adjacent same-glyph shorelines correctly: PASS");
}

static void melt_preview(void) {
    character_dungeon=false;
    for(int y=0;y<32;y++)for(int x=0;x<32;x++) {
        cave_feat[y][x]=FEAT_FLOOR;cave_info[y][x]=CAVE_MARK|CAVE_SEEN;
        cave_m_idx[y][x]=cave_o_idx[y][x]=0;cave_color[y][x]=COLOR_STYLE_BASE;
    }
    style_info[0].floor_row=35;style_info[0].floor_col=0;
    style_info[0].floor_count=4;style_info[0].floor_tiled=true;
    for(int i=0;i<4;i++) {style_info[0].floor_rowv[i]=35;style_info[0].floor_colv[i]=i*2;}
    for(int y=5;y<13;y++)for(int x=5;x<21;x++) {
        if((y==5||y==12)&&(x<7||x>18))continue;
        cave_feat[y][x]=((x+2*y)%5==0||x==12)?FEAT_MELTING_ICE:FEAT_ICE;
    }
    cave_feat[8][15]=FEAT_WATER;cave_feat[9][15]=FEAT_WATER;
    cave_feat[8][16]=FEAT_DEEP_WATER;cave_feat[9][16]=FEAT_WATER;
    cave_feat[14][7]=cave_feat[14][8]=FEAT_MELTING_ICE;
    cave_feat[14][9]=FEAT_ICE;cave_feat[14][18]=FEAT_MELTING_ICE;
    character_dungeon=character_generated=true;
    SDL_Texture* previous=SDL_GetRenderTarget(g_state.renderer);
    SDL_Texture* target=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,640,416);assert(target);
    SDL_SetRenderTarget(g_state.renderer,target);
    for(int frame=0;frame<4;frame++) {
        frame_tick=frame*8;
        SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255);SDL_RenderClear(g_state.renderer);
        for(int y=3;y<16;y++)for(int x=3;x<23;x++) {
            byte a,ta;char c,tc;map_info(y,x,&a,&c,&ta,&tc);
            SDL_FRect dst={(x-3)*32,(y-3)*32,32,32};
            sdl_draw_map_tile_layers_at(y,x,a,c,ta,tc,&dst);
        }
        SDL_Surface* scene=SDL_RenderReadPixels(g_state.renderer,NULL);assert(scene);
        char path[192];strnfmt(path,sizeof(path),
            "scripts/output/melting-ice-render-check/connected-ice-frame%d.png",frame);
        assert(IMG_SavePNG(scene,path));SDL_DestroySurface(scene);
    }
    SDL_SetRenderTarget(g_state.renderer,previous);SDL_DestroyTexture(target);
    puts("Connected ice preview: four frames rendered through production map_info and SDL map layers: PASS");
}

static void melt_snow_bank_test(void) {
    FILE* source=fopen("lib/edit/style-levels.txt","r");assert(source);
    header header={0};char line[2048];int matched=0;
    while(fgets(line,sizeof(line),source)) {
        line[strcspn(line,"\r\n")]=0;
        if(!strncmp(line,"R:86:",5)||!strncmp(line,"R:102:",6)) {
            assert(parse_style_levels(line,&header)==0);matched++;
        }
    }
    fclose(source);assert(matched==2);
    byte solid_row,solid_col,melt_row,melt_col;
    assert(styles_floor_border(FEAT_ICE,&solid_row,&solid_col));
    assert(styles_floor_border(FEAT_MELTING_ICE,&melt_row,&melt_col));
    assert(solid_row==35 && solid_col==0 && melt_row==solid_row && melt_col==solid_col);
    melt_neighbors(FEAT_MELTING_ICE,0,0);
    byte a;char c;map_info_terrain(10,12,&a,&c);
    assert((a&TILE_INDEX_MASK)==35 && ((byte)c&TILE_INDEX_MASK)==0);
    cave_feat[10][11]=FEAT_ICE;
    byte solid_a;char solid_c;map_info_terrain(10,12,&solid_a,&solid_c);
    assert(a==solid_a && c==solid_c);
    puts("Parsed source snow-bank rules and production non-snow floor appearance agree for solid and melting ice: PASS");
}

static void melting_ice_render_tests(void) {
    sdl_idle_animation_shutdown(); /* Verify a scene with only melting ice animates. */
    cave_when=calloc(MAX_DUNGEON_HGT,sizeof(*cave_when));
    cave_natural=calloc(MAX_DUNGEON_HGT,sizeof(*cave_natural));
    mon_list=calloc(64,sizeof(*mon_list)); r_info=calloc(64,sizeof(*r_info));
    o_list=calloc(64,sizeof(*o_list)); k_info=calloc(64,sizeof(*k_info));
    inventory=calloc(INVEN_TOTAL,sizeof(*inventory)); l_list=calloc(64,sizeof(*l_list));
    mon_max=1;
    character_dungeon=false; cave_fixtures_clear();
    p_ptr->cur_map_hgt=p_ptr->cur_map_wid=32;
    p_ptr->py=p_ptr->px=15; p_ptr->wy=p_ptr->wx=0;
    p_ptr->blind=p_ptr->rage=p_ptr->leaping=false;
    p_ptr->update=p_ptr->redraw=p_ptr->window=0;
    g_labyrinth_view_active=false;
    for(int y=0;y<32;y++)for(int x=0;x<32;x++) {
        cave_feat[y][x]=FEAT_FLOOR; cave_info[y][x]=CAVE_MARK|CAVE_SEEN;
        cave_color[y][x]=COLOR_STYLE_BASE; cave_light[y][x]=2;
        cave_m_idx[y][x]=cave_o_idx[y][x]=0;
    }
    f_info[FEAT_MELTING_ICE].x_attr=TILE_FLAG;
    f_info[FEAT_MELTING_ICE].x_char=(char)(TILE_FLAG|1);
    f_info[FEAT_WATER].x_char=f_info[FEAT_DEEP_WATER].x_char=(char)(TILE_FLAG|1);
    character_dungeon=character_generated=true;
    use_bigtile=false; Term->soft_cursor=false;
    cave_feat[10][11]=FEAT_MELTING_ICE;
    frame_tick=0; Term->total_erase=true; prt_map(); Term_fresh(); frame_tick=0;
    lite_spot(10,11); Term_fresh();
    assert(melting_ice_texture && !water_texture && cell_count==1);
    assert(cell_can_animate(&cells[0]) && liquid_frame_count(FEAT_MELTING_ICE)==4);
    SDL_Surface* frames[4];
    for(int frame=0;frame<4;frame++) {
        frame_tick=frame*8; force_map_redraw(); Term_fresh();
        frames[frame]=capture(102+frame); melt_expect_source(frames[frame],frame);
    }
    /* A tiny island can clip a native pocket completely for one frame. */
    assert(!same_surface(frames[0],frames[1]) || !same_surface(frames[0],frames[2])
        || !same_surface(frames[0],frames[3]));
    frame_tick=0; force_map_redraw(); Term_fresh();
    int loads=image_loads,textures=texture_creations,allocs=allocations;
    u64b rng=Rand_state_export(); s32b turns=turn;
    for(int frame=1;frame<=4;frame++) {
        assert(sdl_idle_animation_timeout_ms(frame*8*IDLE_STEP_NS)==0);
        sdl_idle_animation_update(frame*8*IDLE_STEP_NS);
        SDL_Surface* animated=capture(106);
        assert(same_surface(animated,frames[frame%4])); SDL_DestroySurface(animated);
    }
    assert(image_loads==loads && texture_creations==textures && allocations==allocs);
    assert(Rand_state_export()==rng && turn==turns);
    for(int frame=0;frame<4;frame++)SDL_DestroySurface(frames[frame]);
    puts("Melting ice: all four connected atlas frames match pixel-for-pixel; isolated idle loop has no turn/RNG/I/O/allocation effects: PASS");

    k_info[3].x_attr=TILE_FLAG;k_info[3].x_char=(char)(TILE_FLAG|2);
    o_list[1].k_idx=3;o_list[1].tval=TV_SWORD;o_list[1].number=1;
    o_list[1].iy=10;o_list[1].ix=11;
    o_list[1].marked=true;cave_o_idx[10][11]=1;
    r_info[1].x_attr=TILE_FLAG;r_info[1].x_char=(char)(TILE_FLAG|2);
    mon_list[1].r_idx=1;mon_list[1].fy=10;mon_list[1].fx=11;
    mon_list[1].ml=true;mon_list[1].alertness=ALERTNESS_ALERT;
    for(int occupant=0;occupant<2;occupant++) {
        cave_o_idx[10][11]=occupant?0:1; cave_m_idx[10][11]=occupant?1:0;
        frame_tick=0;force_map_redraw();Term_fresh();
        byte a,ta;char c,tc;map_info(10,11,&a,&c,&ta,&tc);
        /* Alert monsters legitimately add GRAPHICS_ALERT_MASK to the glyph. */
        assert(((byte)c&TILE_FLAG) && ((byte)c&TILE_INDEX_MASK)==2);
        assert(cell_can_animate(&cells[0]));
        sdl_idle_animation_update(8*IDLE_STEP_NS);melt_expect_repaint();
        for(int water=0;water<2;water++) {
            cave_set_feat(10,11,water?FEAT_DEEP_WATER:FEAT_WATER);melt_expect_repaint();
            cave_set_feat(10,11,FEAT_MELTING_ICE);melt_expect_repaint();
        }
    }
    cave_o_idx[10][11]=cave_m_idx[10][11]=0;
    for(int big=0;big<2;big++) {
        use_bigtile=big;Term->total_erase=true;
        for(int pan=0;pan<4;pan++) {
            p_ptr->wx=pan%2;p_ptr->wy=pan/2;prt_map();melt_expect_repaint();
        }
    }
    use_bigtile=false;p_ptr->wx=p_ptr->wy=0;
    cave_info[10][11]=CAVE_MARK;Term->total_erase=true;prt_map();Term_fresh();
    assert(visible_liquid(10,11)==FEAT_MELTING_ICE);
    assert(sdl_idle_animation_timeout_ms(1000*IDLE_STEP_NS)==-1);
    cave_info[10][11]=0;lite_spot(10,11);melt_expect_repaint();
    assert(!visible_liquid(10,11));
    puts("Melting ice: object/monster foreground survives idle and shallow/deep replacement; both tile widths pan cleanly; remembered/unseen state respected: PASS");
    melt_connection_tests();melt_snow_bank_test();melt_preview();
}
'''


def check_authored_borders():
    graf = ROOT / "lib/xtra/graf"
    solid = Image.open(graf / "ice_sheet.png").convert("RGBA")
    shores = Image.open(graf / "transition_ice_on_snow.png").convert("RGBA")
    melting = Image.open(graf / "transition_melting_ice_on_snow.png").convert("RGBA")
    assert melting.size == (256, 1024)
    native = [Image.open(graf / f"anim_ice_melt_f{i}.png").convert("RGBA") for i in range(4)]
    changed = 0
    for mask in range(256):
        left, top = mask % 16 * 16, mask // 16 * 16
        for y in range(16):
            for x in range(16):
                authored = shores.getpixel((left + x, top + y))
                # Every shoreline pixel and its one-pixel neighborhood are
                # protected, as are all shared cell edges, even in full ice.
                protected = x in (0, 15) or y in (0, 15) or any(
                    shores.getpixel((left + nx, top + ny)) != solid.getpixel((nx, ny))
                    for ny in range(max(0, y - 1), min(16, y + 2))
                    for nx in range(max(0, x - 1), min(16, x + 2)))
                for frame in range(4):
                    actual = melting.getpixel((left + x, top + y + frame * 256))
                    if protected:
                        assert actual == authored, (mask, frame, x, y, "altered border")
                    elif actual != authored:
                        assert actual == native[frame].getpixel((x, y))
                        changed += 1
    assert changed > 0
    print("All atlas masks/frames preserve authored ice shorelines, their one-pixel buffer and shared cell edges; animated interior pixels come from native melt frames: PASS")


def main():
    check_authored_borders()
    OUT.mkdir(parents=True, exist_ok=True)
    idle.OUT.mkdir(parents=True, exist_ok=True)
    idle.HARNESS = idle.HARNESS.replace("int main(void) {", TESTS + "\nint main(void) {")
    idle.HARNESS = idle.HARNESS.replace(
        "    sdl_idle_animation_shutdown();\n    SDL_Quit();",
        "    melting_ice_render_tests();\n    sdl_idle_animation_shutdown();\n    SDL_Quit();")
    idle.OUT = OUT
    idle.main()
    frames = [Image.open(OUT / f"connected-ice-frame{i}.png").convert("RGB") for i in range(4)]
    frames[0].save(OUT / "connected-ice.png")
    frames[0].save(OUT / "connected-ice.gif", save_all=True, append_images=frames[1:],
                   duration=320, loop=0)


if __name__ == "__main__":
    main()

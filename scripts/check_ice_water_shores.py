#!/usr/bin/env python3
"""Render and validate narrow ice channels before, during and after melting.

Build first. Reuses isolated production SDL harnesses; requires Pillow.
Outputs three PNG stages, their animated frames, and an animated comparison.
"""
from pathlib import Path
from PIL import Image, ImageDraw
import check_idle_animation as idle
import check_melting_ice_render as melting

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "scripts/output/ice-water-shores-check"

TESTS = r'''
static bool icy_channel_cell(int y,int x) {
    return (y>=5&&y<=6&&x>=5&&x<=8)
        || (y>=5&&y<=8&&x>=7&&x<=8)
        || (y>=7&&y<=8&&x>=7&&x<=12)
        || (y>=7&&y<=10&&x>=11&&x<=12)
        || (y>=9&&y<=10&&x>=11&&x<=18)
        || (y>=10&&y<=15&&x>=17&&x<=22);
}

static byte icy_channel_feature(int y,int x,int stage) {
    if(!icy_channel_cell(y,x))return FEAT_FLOOR;
    bool broken=stage==2||(stage==1&&((x+y)%3!=0));
    if(broken)return x>=19&&x<=20&&y>=12&&y<=13?FEAT_DEEP_WATER:FEAT_WATER;
    return (x+2*y)%4?FEAT_MELTING_ICE:FEAT_ICE;
}

static void icy_channel_reset(void) {
    character_dungeon=false;use_bigtile=false;
    p_ptr->wx=p_ptr->wy=0;p_ptr->py=p_ptr->px=28;
    p_ptr->blind=p_ptr->rage=p_ptr->image=false;
    cave_fixtures_clear();cave_water_flow_reset();sdl_idle_animation_clear_cells();
    for(int y=0;y<32;y++)for(int x=0;x<32;x++) {
        cave_feat[y][x]=FEAT_FLOOR;cave_info[y][x]=CAVE_MARK|CAVE_SEEN;
        cave_light[y][x]=2;cave_color[y][x]=COLOR_STYLE_BASE;
        cave_m_idx[y][x]=cave_o_idx[y][x]=0;
    }
    style_info[0].floor_row=35;style_info[0].floor_col=0;
    style_info[0].floor_count=4;style_info[0].floor_tiled=true;
    for(int i=0;i<4;i++) {
        style_info[0].floor_rowv[i]=35;style_info[0].floor_colv[i]=i*2;
    }
    character_dungeon=character_generated=true;
    Term->total_erase=true;prt_map();Term_fresh();
}

static void icy_channel_capture(int stage,int frame) {
    Uint64 previous_tick=frame_tick;
    SDL_Texture* previous=SDL_GetRenderTarget(g_state.renderer);
    SDL_Texture* target=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,704,448);assert(target);
    SDL_SetRenderTarget(g_state.renderer,target);frame_tick=frame*8;
    SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255);SDL_RenderClear(g_state.renderer);
    for(int y=3;y<17;y++)for(int x=3;x<25;x++) {
        byte a,ta;char c,tc;map_info(y,x,&a,&c,&ta,&tc);
        SDL_FRect dst={(x-3)*32,(y-3)*32,32,32};
        sdl_draw_map_tile_layers_at(y,x,a,c,ta,tc,&dst);
    }
    SDL_Surface* scene=SDL_RenderReadPixels(g_state.renderer,NULL);assert(scene);
    char path[192];strnfmt(path,sizeof(path),
        "scripts/output/ice-water-shores-check/stage%d-frame%d.png",stage,frame);
    assert(IMG_SavePNG(scene,path));SDL_DestroySurface(scene);
    SDL_SetRenderTarget(g_state.renderer,previous);SDL_DestroyTexture(target);
    frame_tick=previous_tick;
}

static void icy_surface_connectivity_tests(void) {
    icy_channel_reset();
    const byte features[]={FEAT_ICE,FEAT_MELTING_ICE,FEAT_WATER,FEAT_DEEP_WATER};
    for(int kind=0;kind<4;kind++)for(int phase=0;phase<4;phase++)
        for(int mask=0;mask<256;mask++) {
            cave_feat[10][11]=features[kind];int frozen=0;
            for(int i=0;i<8;i++) {
                int y=10+melt_dy[i],x=11+melt_dx[i];
                cave_feat[y][x]=(mask&(1<<i))?features[(i+phase)%4]:FEAT_FLOOR;
                cave_info[y][x]=CAVE_MARK|CAVE_SEEN;
                if(FEAT_IS_ICE(cave_feat[y][x]))frozen|=1<<i;
            }
            assert(liquid_transition_mask(10,11,features[kind])==mask);
            assert(ice_only_transition_mask(10,11)==frozen);
        }
    for(int i=0;i<8;i++)cave_info[10+melt_dy[i]][11+melt_dx[i]]=0;
    assert(liquid_transition_mask(10,11,FEAT_DEEP_WATER)==0);
    assert(ice_only_transition_mask(10,11)==0);
    SDL_Surface* unknown=melt_sample(0,NULL,0);
    for(int i=0;i<8;i++)cave_feat[10+melt_dy[i]][11+melt_dx[i]]=FEAT_FLOOR;
    SDL_Surface* absent=melt_sample(0,NULL,0);
    assert(same_surface(unknown,absent));
    SDL_DestroySurface(unknown);SDL_DestroySurface(absent);
    puts("All 256 masks x four feature rotations x four centers connect ice/melting/shallow/deep externally and keep a separate ice-only rim; hidden neighbors reveal no pixels: PASS");

    icy_channel_reset();cave_feat[10][11]=FEAT_ICE;
    for(int i=0;i<8;i++)cave_feat[10+melt_dy[i]][11+melt_dx[i]]=FEAT_WATER;
    frame_tick=0;force_map_redraw();Term_fresh();frame_tick=0;
    force_map_redraw();Term_fresh();
    assert(liquid_frame_count_at(10,11,FEAT_ICE)==4);
    bool tracked=false;
    for(int i=0;i<cell_count;i++)if(cells[i].y==10&&cells[i].x==11) {
        assert(cell_can_animate(&cells[i]));tracked=true;
    }
    assert(tracked);
    SDL_Surface* before=melt_sample(0,NULL,0);
    SDL_Surface* after=melt_sample(1,NULL,0);
    assert(!same_surface(before,after));
    cave_feat[10][11]=FEAT_WATER;
    SDL_Surface* water_only=melt_sample(0,NULL,0);
    assert(!same_surface(before,water_only));
    SDL_DestroySurface(water_only);cave_feat[10][11]=FEAT_ICE;
    SDL_DestroySurface(before);SDL_DestroySurface(after);
    frame_tick=0;force_map_redraw();Term_fresh();
    int loads=image_loads,textures=texture_creations,allocs=allocations;
    u64b rng=Rand_state_export();s32b turns=turn;
    for(int frame=1;frame<=12;frame++)sdl_idle_animation_update(frame*8*IDLE_STEP_NS);
    assert(loads==image_loads&&textures==texture_creations&&allocs==allocations);
    assert(rng==Rand_state_export()&&turns==turn);melt_expect_repaint();
    puts("Static ice's exposed water rim animates while idle with no RNG/turn/I/O/allocation changes, matching complete repaint: PASS");
}

static void icy_channel_redraw_tests(void) {
    icy_channel_reset();
    for(int stage=0;stage<3;stage++) {
        for(int y=3;y<17;y++) {
            for(int x=3;x<25;x++)
            if(icy_channel_cell(y,x)) {
                cave_set_feat(y,x,icy_channel_feature(y,x,stage));
            }
            melt_expect_repaint();
        }
        for(int frame=0;frame<4;frame++)icy_channel_capture(stage,frame);
    }
    /* The map remains snow-bordered after the final ice has disappeared. */
    for(int y=3;y<17;y++)for(int x=3;x<25;x++)assert(!FEAT_IS_ICE(cave_feat[y][x]));
    const int changed_y[]={5,6,7,8,9,10};
    const int changed_x[]={7,7,9,11,13,17};
    for(int round=0;round<3;round++)for(int stage=0;stage<3;stage++)
        for(int i=0;i<6;i++) {
            int y=changed_y[i],x=changed_x[i];
            cave_set_feat(y,x,icy_channel_feature(y,x,stage));melt_expect_repaint();
        }
    for(int big=0;big<2;big++)for(int pan=0;pan<4;pan++) {
        use_bigtile=big;p_ptr->wx=pan%2;p_ptr->wy=pan/2;
        if(!pan)Term->total_erase=true;prt_map();melt_expect_repaint();
    }
    use_bigtile=false;p_ptr->wx=p_ptr->wy=0;
    puts("Narrow stair-step channels and connected pool: successive breaks, full thaw, repeated restoration and both tile-width pans match full redraw: PASS");
}
'''


def check_preview():
    labels = ("Before melting", "Partly melted", "Fully melted")
    stage_frames = []
    for stage, label in enumerate(labels):
        frames = [Image.open(OUT / f"stage{stage}-frame{frame}.png").convert("RGB")
                  for frame in range(4)]
        for frame, picture in enumerate(frames):
            pixels = picture.tobytes()
            brown = sum(pixels[i] > pixels[i + 2] + 4 and pixels[i + 1] > pixels[i + 2] + 4
                        for i in range(0, len(pixels), 3))
            assert brown == 0, (stage, frame, brown, "brown land/bank pixels in snow-and-ice scene")
        frames[0].save(OUT / f"{('before', 'partial', 'all-melted')[stage]}.png")
        frames[0].save(OUT / f"stage{stage}.gif", save_all=True, append_images=frames[1:],
                       duration=320, loop=0)
        labeled = []
        for frame in frames:
            picture = Image.new("RGB", (frame.width, frame.height + 30), "#152d40")
            picture.paste(frame, (0, 30))
            ImageDraw.Draw(picture).text((12, 9), label, fill="white")
            labeled.append(picture)
        stage_frames.append(labeled)
    animated = [frame for stage in stage_frames for frame in stage * 2]
    animated[0].save(OUT / "ice-melting-stages.gif", save_all=True,
                     append_images=animated[1:], duration=320, loop=0)
    comparison = Image.new("RGB", (704 * 3, 478))
    for stage in range(3):
        comparison.paste(stage_frames[stage][0], (704 * stage, 0))
    comparison.save(OUT / "ice-melting-stages.png")
    print("Every SDL frame before, during and after full thaw has zero brown bank pixels: PASS")


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    melting.OUT.mkdir(parents=True, exist_ok=True)
    idle.OUT.mkdir(parents=True, exist_ok=True)
    idle.HARNESS = idle.HARNESS.replace("int main(void) {",
                                       melting.TESTS + TESTS + "\nint main(void) {")
    idle.HARNESS = idle.HARNESS.replace(
        "    sdl_idle_animation_shutdown();\n    SDL_Quit();",
        "    melting_ice_render_tests();\n    icy_surface_connectivity_tests();\n"
        "    icy_channel_redraw_tests();\n"
        "    sdl_idle_animation_shutdown();\n    SDL_Quit();")
    idle.OUT = OUT
    idle.main()
    check_preview()


if __name__ == "__main__":
    main()

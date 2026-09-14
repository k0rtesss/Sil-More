#!/usr/bin/env python3
"""Production map/SDL checks for snow caves and basalt lava banks.

Build first with build-incremental.ps1. Runs isolated fixtures, never saves.
Writes a production-rendered snow/ice and lava-pool/river preview to output.
"""
import check_idle_animation as idle
import json
import tempfile
from pathlib import Path

TESTS = r'''
#include "init.h"
#include "init/init-parse-internal.h"
#include "init/init2-internal.h"
static void floor_startup(const char* edit, const char* data) {
    const char* old_edit=ANGBAND_DIR_EDIT; const char* old_data=ANGBAND_DIR_DATA;
    maxima* old_limits=z_info; style_type* old_styles=style_info;
    char* old_names=style_name;
    ANGBAND_DIR_EDIT=edit; ANGBAND_DIR_DATA=data;
    /* Exercise the actual startup allocation and raw-cache load paths. */
    for(int pass=0;pass<2;pass++) {
        assert(init_z_info()==0);
        assert(z_info->style_max==64);
        assert(init_style_info()==0);
        style_info=style_head.info_ptr;
        assert(style_head.info_num==z_info->style_max);
        assert(style_info[62].name && style_info[62].floor_tiled);
        assert(style_info[63].name && style_info[63].floor_row==35);
        free_info(&style_head); free_info(&z_head);
    }
    z_info=old_limits; style_info=old_styles; style_name=old_names;
    ANGBAND_DIR_EDIT=old_edit; ANGBAND_DIR_DATA=old_data;
    printf("Production style startup, fresh and cached: %s: PASS\n",edit);
}
static void floor_templates(void) {
    if(!r_info) r_info=calloc(1,sizeof(*r_info));
    if(!inventory) inventory=calloc(INVEN_TOTAL,sizeof(*inventory));
    f_info[FEAT_LAVA].x_char=f_info[FEAT_ICE].x_char=(char)(TILE_FLAG|1);
    header h={0}, limits={0}; limits.info_ptr=z_info;
    FILE* limits_file=fopen("lib/edit/limits.txt","r"); assert(limits_file);
    char limit_line[2048];
    while(fgets(limit_line,sizeof(limit_line),limits_file)) {
        limit_line[strcspn(limit_line,"\r\n")]=0;
        if(limit_line[0]=='M') assert(parse_z_info(limit_line,&limits)==0);
    }
    fclose(limits_file);
    assert(z_info->style_max>63 && z_info->style_max<=64);
    free(style_info); style_info=calloc(z_info->style_max,sizeof(*style_info));
    h.info_num=z_info->style_max; h.info_len=sizeof(*style_info); h.info_ptr=style_info;
    h.name_ptr=calloc(z_info->fake_name_size,1); h.text_ptr=calloc(z_info->fake_text_size,1);
    FILE* f=fopen("lib/edit/style.txt","r"); assert(f);
    char line[2048]; error_idx=-1;
    while(fgets(line,sizeof(line),f)) {
        line[strcspn(line,"\r\n")]=0;
        if(!line[0] || line[0]=='#' || line[0]=='V') continue;
        errr result=parse_style_info(line,&h);
        if(result) fprintf(stderr,"Style parse error %d: %s\n",result,line);
        assert(result==0);
    }
    fclose(f);
    assert(style_info[62].floor_count==4);
    for(int i=0;i<4;i++) assert(style_info[62].floor_rowv[i]==35 && style_info[62].floor_colv[i]==2*i);
    assert(style_info[63].floor_row==35 && style_info[63].floor_col==8);
    f=fopen("lib/edit/style-levels.txt","r"); assert(f);
    while(fgets(line,sizeof(line),f)) {
        line[strcspn(line,"\r\n")]=0;
        assert(parse_style_levels(line,&h)==0);
    }
    fclose(f);
    for(int depth=1;depth<=20;depth++) for(int n=0;n<100;n++) {
        assert(styles_pick_partition_style(depth,PART_STYLE_BIG_CAVE_ICE)==62);
        assert(styles_pick_partition_style(depth,PART_STYLE_BIG_CAVE_FIRE)==63);
    }
    p_ptr->depth=5; styles_init_for_level();
    puts("Source templates, snow variants and elemental partition selection at every dungeon depth: PASS");
}
static void floor_reset(int style) {
    cave_fixtures_clear(); sdl_idle_animation_clear_cells();
    use_bigtile=false; p_ptr->wx=p_ptr->wy=0;
    p_ptr->blind=p_ptr->rage=p_ptr->image=0; p_ptr->is_dead=false;
    g_labyrinth_view_active=false;
    for(int y=0;y<32;y++) for(int x=0;x<32;x++) {
        cave_feat[y][x]=FEAT_FLOOR; cave_info[y][x]=CAVE_MARK|CAVE_SEEN;
        cave_color[y][x]=COLOR_STYLE_BASE+style; cave_light[y][x]=2;
        cave_m_idx[y][x]=cave_o_idx[y][x]=0;
    }
}
static int floor_tile(int y,int x) {
    byte a,ta; char c,tc; map_info(y,x,&a,&c,&ta,&tc);
    return ((ta&TILE_INDEX_MASK)<<8)|((byte)tc&TILE_INDEX_MASK);
}
static void floor_tests(void) {
    floor_reset(62); u64b rng=Rand_state_export(); s32b turns=turn;
    int variants=0;
    for(int y=2;y<24;y++) for(int x=2;x<24;x++) {
        int tile=floor_tile(y,x); assert(tile>>8==35);
        assert((tile&255)<=6 && !(tile&1)); variants|=1<<((tile&255)/2);
        for(int n=0;n<10;n++) assert(floor_tile(y,x)==tile);
    }
    assert(variants==15 && Rand_state_export()==rng && turn==turns);
    int lit=floor_tile(10,11);
    cave_info[10][11]=CAVE_MARK; cave_light[10][11]=2;
    assert(floor_tile(10,11)==lit+1);
    cave_info[10][11]=CAVE_MARK|CAVE_SEEN;
    cave_feat[10][11]=FEAT_LESS; assert(floor_tile(10,11)==lit);
    cave_feat[10][11]=FEAT_FLOOR;
    cave_m_idx[10][11]=-1; assert(floor_tile(10,11)==lit);
    floor_reset(63); assert(floor_tile(10,11)==((35<<8)|8));
    const int dy[8]={-1,-1,0,1,1,1,0,-1},dx[8]={0,1,1,1,0,-1,-1,-1};
    for(int i=0;i<8;i++) {
        floor_reset(13); int normal=floor_tile(10,11);
        cave_feat[10+dy[i]][11+dx[i]]=FEAT_LAVA;
        assert(floor_tile(10,11)==((35<<8)|8));
        assert(floor_tile(20,21)==normal);
        cave_info[10+dy[i]][11+dx[i]]=0; assert(floor_tile(10,11)==normal);
        cave_info[10+dy[i]][11+dx[i]]=CAVE_MARK;
        assert(floor_tile(10,11)==((35<<8)|8));
        p_ptr->rage=1; assert(floor_tile(10,11)==normal); p_ptr->rage=0;
        g_labyrinth_view_active=true; assert(floor_tile(10,11)==normal);
        g_labyrinth_view_active=false;
        cave_info[10+dy[i]][11+dx[i]]=CAVE_MARK|CAVE_SEEN;
        cave_feat[10+dy[i]][11+dx[i]]=FEAT_BRIDGE_LAVA_H;
        assert(floor_tile(10,11)==((35<<8)|8));
    }
    floor_reset(13); cave_feat[9][11]=FEAT_LAVA;
    cave_feat[10][11]=FEAT_LESS; assert(floor_tile(10,11)==((35<<8)|8));
    cave_info[10][11]=CAVE_MARK; cave_light[10][11]=0;
    assert(floor_tile(10,11)==((35<<8)|9));
    assert(Rand_state_export()==rng && turn==turns);
    puts("Stable varied snow, floor/feature/actor underlays, dark variants, eight-way basalt banks, hidden/rage/labyrinth knowledge: PASS");
}
static void floor_redraw_tests(void) {
    floor_reset(13); cave_feat[9][11]=FEAT_LAVA; cave_info[9][11]=0;
    Term->total_erase=true; prt_map(); Term_fresh();
    cave_info[9][11]=CAVE_MARK|CAVE_SEEN; lite_spot(9,11); Term_fresh();
    SDL_Surface* changed=capture(80);
    force_map_redraw(); Term_fresh(); SDL_Surface* full=capture(81);
    assert(same_surface(changed,full));
    SDL_DestroySurface(changed); SDL_DestroySurface(full);
    cave_set_feat(9,11,FEAT_FLOOR); Term_fresh(); changed=capture(82);
    force_map_redraw(); Term_fresh(); full=capture(83);
    assert(same_surface(changed,full));
    SDL_DestroySurface(changed); SDL_DestroySurface(full);
    puts("Lava reveal/removal incremental repaint matches full map repaint: PASS");
}
static void floor_preview(void) {
    const int w=24,h=18,scale=2;
    SDL_Texture* previous=SDL_GetRenderTarget(g_state.renderer);
    SDL_Texture* target=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,w*16*scale*2,h*16*scale); assert(target);
    SDL_SetRenderTarget(g_state.renderer,target);
    for(int panel=0;panel<2;panel++) {
        floor_reset(panel==0?62:13);
        for(int y=0;y<h;y++) for(int x=0;x<w;x++) {
            bool wall=y<2||y>=h-2||x<2||x>=w-2 || (y==2 && x%5<2);
            bool pool=(y>=4&&y<=9&&x>=4&&x<=9 && !(y==4&&x==4));
            bool river=(x>=15+(y/4)%2 && x<=17+(y/4)%2);
            cave_feat[y][x]=wall?FEAT_WALL_EXTRA:(pool||river)?(panel==0?FEAT_ICE:FEAT_LAVA):FEAT_FLOOR;
            cave_info[y][x]=CAVE_MARK|CAVE_SEEN|CAVE_GLOW|(wall?CAVE_WALL:0);
        }
        for(int y=0;y<h;y++) for(int x=0;x<w;x++) {
            byte a,ta; char c,tc; map_info(y,x,&a,&c,&ta,&tc);
            SDL_FRect dst={(panel*w+x)*16*scale,y*16*scale,16*scale,16*scale};
            sdl_draw_map_tile_layers_at(y,x,a,c,ta,tc,&dst);
        }
    }
    SDL_Surface* surface=SDL_RenderReadPixels(g_state.renderer,NULL); assert(surface);
    /* Both production surface textures must actually have loaded/drawn. */
    assert(ice_texture && lava_texture);
    assert(IMG_SavePNG(surface,"scripts/output/cave-floor-tiles-check/preview.png"));
    SDL_DestroySurface(surface); SDL_SetRenderTarget(g_state.renderer,previous);
    SDL_DestroyTexture(target);
}
'''


def main():
    idle.OUT = idle.ROOT / "scripts/output/cave-floor-tiles-check"
    idle.HARNESS = idle.HARNESS.replace("scripts/output/idle-animation-check",
                                      "scripts/output/cave-floor-tiles-check")
    idle.HARNESS = idle.HARNESS.replace("int main(void) {", TESTS + "\nint main(void) {")
    idle.HARNESS = idle.HARNESS.replace("    asynchronous_tests();",
        "    asynchronous_tests();\n    floor_templates();\n    floor_tests();\n    floor_redraw_tests();\n    floor_preview();")
    with tempfile.TemporaryDirectory(prefix="sil-style-startup-") as temporary:
        calls = []
        for index, root in enumerate((idle.ROOT, idle.ROOT / "sil-more-windows-sdl3")):
            data = Path(temporary) / str(index)
            data.mkdir()
            calls.append("    floor_startup(%s,%s);" % (
                json.dumps(str(root / "lib/edit")), json.dumps(str(data))))
        idle.HARNESS = idle.HARNESS.replace("    floor_preview();",
            "    floor_preview();\n" + "\n".join(calls))
        idle.main()
        for index in range(2):
            for name in ("limits.raw", "style.raw"):
                assert (Path(temporary) / str(index) / name).is_file()


if __name__ == "__main__":
    main()

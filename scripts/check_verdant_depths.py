#!/usr/bin/env python3
"""Exercise plain chasm rendering and Verdant wall styles with existing floors.

Build first with build-incremental.ps1. Uses the production map/SDL code with
isolated maps, parses source templates, and never opens player saves.
"""
import re
import check_idle_animation as idle

TESTS = r'''
#include "init.h"
#include "init/init-parse-internal.h"
int cave_style_primary_for_grid(int y, int x);
static void verdant_style_tests(void) {
    header h = {0};
    z_info->style_max=64;
    z_info->fake_name_size=z_info->fake_text_size=131072;
    free(style_info); style_info=calloc(64,sizeof(*style_info));
    h.info_num=64; h.info_len=sizeof(*style_info); h.info_ptr=style_info;
    h.name_ptr=calloc(131072,1); h.text_ptr=calloc(131072,1);
    FILE* file=fopen("lib/edit/style.txt","r"); assert(file);
    char line[2048]; error_idx=-1;
    while(fgets(line,sizeof(line),file)) {
        line[strcspn(line,"\r\n")]=0;
        if(!line[0] || line[0]=='#' || line[0]=='V') continue;
        assert(parse_style_info(line,&h)==0);
    }
    fclose(file);
    for(int i=40;i<=42;i++) {
        assert(style_info[i].name && style_info[i].wall_row==33);
        assert(style_info[i].wall_col==(i-40)*2);
        assert(style_info[i].floor_count==1 && style_info[i].floor_row==23
            && style_info[i].floor_col==0);
    }
    file=fopen("lib/edit/style-levels.txt","r"); assert(file);
    while(fgets(line,sizeof(line),file)) {
        line[strcspn(line,"\r\n")]=0;
        assert(parse_style_levels(line,&h)==0);
    }
    fclose(file);
    bool found_flag=false, found_moss=false;
    /* The configured playable depths use the existing L:/P: rules. */
    for(int depth=1;depth<=MORGOTH_DEPTH;depth++) {
        p_ptr->depth=depth;
        for(int n=0;n<300;n++) {
            styles_init_for_level();
            int style=cave_style_primary_for_grid(10,11);
            assert(style<40 || style>42);
            style=styles_pick_random_from_level(); /* Also used by unstyled vaults and '$'. */
            assert(style<40 || style>42);
            for(int kind=0;kind<PART_STYLE_MAX;kind++) {
                style=styles_pick_partition_style(depth,kind);
                assert(style!=41);
                if(style==40) {
                    assert(kind==PART_STYLE_LABYRINTH && depth>=8 && depth<=10);
                    assert(mode_weight_for_depth(QUAD_MODE_LABYRINTH,depth,12,NULL,9)>0);
                    found_flag=true;
                }
                if(style==42) {
                    assert(kind==PART_STYLE_CA_BLOB && depth>=5 && depth<=7);
                    assert(mode_weight_for_depth(QUAD_MODE_CAVEY,depth,12,NULL,9)>0);
                    found_moss=true;
                }
                if(depth<=20 && kind==PART_STYLE_BIG_CAVE_FIRE) assert(style==25);
                if(depth<=20 && kind==PART_STYLE_BIG_CAVE_ICE) assert(style==59);
                if(depth<=20 && kind==PART_STYLE_BIG_CAVE_POIS) assert(style==55);
                if(depth<=20 && kind==PART_STYLE_CHASM_FLOOR) assert(style==30);
            }
        }
    }
    assert(found_flag && found_moss);
    p_ptr->depth=0; styles_init_for_level();
    assert(cave_style_primary_for_grid(10,11)==13);
    p_ptr->depth=5;
    puts("Biome palettes: flagstone only dark stone labyrinths, moss only green caves; general/elemental/Morgoth pools exclude all three: PASS");
}

static void forge_palette_tests(void) {
    for(unsigned i=0;i<N_ELEMENTS(forge_palette_cases);i++) {
        const forge_palette_case* item=&forge_palette_cases[i];
        bool saw_masonry=false, saw_other=false;
        for(int depth=item->min_depth;depth<=item->max_depth;depth++) {
            p_ptr->depth=depth; styles_init_for_level();
            for(int n=0;n<300;n++) {
                choose_test_vault_palette(&item->vault);
                int style=styles_get_vault_primary_style();
                if(style==41) saw_masonry=true; else saw_other=true;
                styles_end_vault();
            }
        }
        assert(saw_masonry && saw_other);
    }
    puts("Masonry: authored forge S: palettes; original alternatives and vault availability retained: PASS");
}

static void chasm_map_reset(void) {
    cave_fixtures_clear(); sdl_idle_animation_clear_cells();
    use_bigtile=false; p_ptr->wx=p_ptr->wy=0; p_ptr->blind=p_ptr->rage=0;
    g_labyrinth_view_active=false;
    for(int y=0;y<32;y++) for(int x=0;x<32;x++) {
        cave_feat[y][x]=FEAT_FLOOR; cave_info[y][x]=CAVE_MARK|CAVE_SEEN;
        cave_color[y][x]=COLOR_STYLE_BASE+40;
        cave_light[y][x]=2; cave_m_idx[y][x]=cave_o_idx[y][x]=0;
    }
    f_info[FEAT_CHASM].x_attr=TILE_FLAG|34;
    f_info[FEAT_CHASM].x_char=(char)TILE_FLAG;
    cave_feat[10][11]=FEAT_CHASM;
}
static int chasm_tile_index(int y,int x) {
    byte a,ta; char c,tc; map_info(y,x,&a,&c,&ta,&tc);
    assert((ta&TILE_FLAG) && ((byte)tc&TILE_FLAG));
    return ((ta&TILE_INDEX_MASK)-34)*16+((byte)tc&TILE_INDEX_MASK)/2;
}
static void chasm_tests(void) {
    const int dy[8]={-1,-1,0,1,1,1,0,-1}, dx[8]={0,1,1,1,0,-1,-1,-1};
    chasm_map_reset();
    u64b rng=Rand_state_export(); s32b turns=turn;
    for(int mask=0;mask<256;mask++) {
        for(int i=0;i<8;i++) cave_feat[10+dy[i]][11+dx[i]]=
            mask&(1<<i)?FEAT_CHASM:FEAT_FLOOR;
        assert(chasm_tile_index(10,11)==0);
    }
    /* Unknown terrain must have the same appearance regardless of its type. */
    cave_info[9][11]=0;
    cave_feat[9][11]=FEAT_FLOOR; int unknown=chasm_tile_index(10,11);
    cave_feat[9][11]=FEAT_CHASM; assert(chasm_tile_index(10,11)==unknown);
    cave_info[9][11]=CAVE_MARK; p_ptr->rage=1;
    cave_feat[9][11]=FEAT_FLOOR; assert(chasm_tile_index(10,11)==unknown);
    p_ptr->rage=0; g_labyrinth_view_active=true;
    assert(chasm_tile_index(10,11)==unknown); g_labyrinth_view_active=false;
    cave_info[10][11]=0;
    byte a,ta; char c,tc; map_info(10,11,&a,&c,&ta,&tc);
    assert((ta&TILE_INDEX_MASK)!=34 && (ta&TILE_INDEX_MASK)!=37);
    cave_info[10][11]=CAVE_MARK;
    map_info(10,11,&a,&c,&ta,&tc); assert(chasm_tile_index(10,11)==0);
    cave_info[10][11]|=CAVE_SEEN; p_ptr->blind=1;
    map_info(10,11,&a,&c,&ta,&tc); assert(chasm_tile_index(10,11)==0);
    p_ptr->blind=0;
    map_info(10,11,&a,&c,&ta,&tc); assert(((byte)tc&TILE_INDEX_MASK)%2==0);
    use_graphics=GRAPHICS_NONE;
    f_info[FEAT_CHASM].d_char='%'; f_info[FEAT_CHASM].d_attr=TERM_L_DARK;
    map_info(10,11,&a,&c,&ta,&tc); assert(c=='%');
    use_graphics=GRAPHICS_MICROCHASM;
    assert(Rand_state_export()==rng && turn==turns);
    /* Revealing/changing nearby terrain retains ordinary map redraws. */
    chasm_map_reset(); cave_info[9][11]=0;
    Term->total_erase=true; prt_map(); Term_fresh();
    SDL_Surface* before=capture(80);
    cave_info[9][11]=CAVE_MARK|CAVE_SEEN; lite_spot(9,11); Term_fresh();
    SDL_Surface* changed=capture(81); assert(!same_surface(before,changed));
    force_map_redraw(); Term_fresh(); SDL_Surface* full=capture(82);
    assert(same_surface(changed,full));
    SDL_DestroySurface(before); SDL_DestroySurface(changed); SDL_DestroySurface(full);
    cave_set_feat(9,11,FEAT_CHASM); Term_fresh(); changed=capture(83);
    force_map_redraw(); Term_fresh(); full=capture(84); assert(same_surface(changed,full));
    SDL_DestroySurface(changed); SDL_DestroySurface(full);
    for(int big=0;big<2;big++) for(int pan=0;pan<3;pan++) {
        if(pan==0) Term->total_erase=true; /* Tile-width changes rebuild the canvas. */
        use_bigtile=big; p_ptr->wx=pan; prt_map(); Term_fresh(); changed=capture(85);
        force_map_redraw(); Term_fresh(); full=capture(86); assert(same_surface(changed,full));
        SDL_DestroySurface(changed); SDL_DestroySurface(full);
    }
    puts("One plain chasm tile for every neighbour layout; visibility, ASCII and map redraws: PASS");
}

static void verdant_preview(void) {
    const int w=18,h=14,scale=2;
    chasm_map_reset();
    SDL_Texture* previous=SDL_GetRenderTarget(g_state.renderer);
    SDL_Texture* target=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,w*16*scale*3,h*16*scale); assert(target);
    SDL_SetRenderTarget(g_state.renderer,target);
    for(int style=40;style<=42;style++) {
        for(int y=0;y<h;y++) for(int x=0;x<w;x++) {
            bool wall=y<2||y>=h-2||x<2||x>=w-2;
            bool pit=y>=4&&y<=9&&x>=5&&x<=12&&!(y==6&&x<10);
            cave_feat[y][x]=wall?FEAT_WALL_EXTRA:pit?FEAT_CHASM:FEAT_FLOOR;
            cave_info[y][x]=CAVE_MARK|CAVE_SEEN|CAVE_GLOW|(wall?CAVE_WALL:0);
            cave_color[y][x]=COLOR_STYLE_BASE+style; cave_light[y][x]=2;
        }
        cave_fixture_set(1,5,CAVE_FIXTURE_WALL_TORCH);
        for(int y=0;y<h;y++) for(int x=0;x<w;x++) {
            byte a,ta; char c,tc; map_info(y,x,&a,&c,&ta,&tc);
            SDL_FRect dst={((style-40)*w+x)*16*scale,y*16*scale,16*scale,16*scale};
            sdl_draw_map_tile_layers_at(y,x,a,c,ta,tc,&dst);
        }
    }
    SDL_Surface* surface=SDL_RenderReadPixels(g_state.renderer,NULL); assert(surface);
    assert(IMG_SavePNG(surface,"scripts/output/verdant-depths-check/preview.png"));
    SDL_DestroySurface(surface); SDL_SetRenderTarget(g_state.renderer,previous);
    SDL_DestroyTexture(target);
}
'''


def main():
    idle.OUT = idle.ROOT / "scripts/output/verdant-depths-check"
    idle.HARNESS = idle.HARNESS.replace("scripts/output/idle-animation-check",
                                      "scripts/output/verdant-depths-check")
    # Exercise the actual selection statements used by build_vault, without
    # placing a vault's monsters, loot or quest state in the rendering fixture.
    source=(idle.ROOT / "src/level-generation/level-generation-rooms-vaults.c").read_text()
    start=source.index("    /* Begin the vault style context now that the vault is accepted */")
    end=source.index("    log_debug(\"build_vault: level_primary",start)
    generation=(idle.ROOT / "src/level-generation/level-generation-internal.h").read_text()
    mode_enum=re.search(r"typedef enum quadrant_mode\s*\{.*?\} quadrant_mode_t;",generation,re.S).group()
    policy=mode_enum+"\nint mode_weight_for_depth(quadrant_mode_t,int,int,const int*,int);\n"
    policy+="static void choose_test_vault_palette(const vault_type* v_ptr) {\n"+source[start:end]+"}\n"
    records=[]
    for block in re.split(r"(?m)^N:", (idle.ROOT / "lib/edit/vault.txt").read_text())[1:]:
        lines=block.splitlines(); serial=int(lines[0].split(":")[0])
        styles=next((line[2:].split() for line in lines if line.startswith("S:")),[])
        if not any(token.startswith("41:") for token in styles): continue
        assert serial in {23,204,269} and any(line.startswith("D:") and "0" in line[2:] for line in lines)
        typ,depth,rarity,*limits=map(int,next(line[2:] for line in lines if line.startswith("X:")).split(":"))
        max_depth=limits[0] if limits and limits[0] else 20
        assert typ!=9 and depth<=max_depth
        indices,weights=zip(*(map(int,token.split(":")) for token in styles))
        records.append('{%d, %d, {.forge=true, .typ=%d, .style_count=%d, '
                       '.style_idx={%s}, .style_weight={%s}}}' %
                       (depth,max_depth,typ,len(styles),','.join(map(str,indices)),','.join(map(str,weights))))
    assert len(records)==3
    policy+='typedef struct {int min_depth, max_depth; vault_type vault;} forge_palette_case;\n'
    policy+='static const forge_palette_case forge_palette_cases[]={'+','.join(records)+'};\n'
    idle.HARNESS = idle.HARNESS.replace("int main(void) {", policy + TESTS + "\nint main(void) {")
    idle.HARNESS = idle.HARNESS.replace("    asynchronous_tests();",
        "    asynchronous_tests();\n    verdant_style_tests();\n    forge_palette_tests();\n    chasm_tests();\n    verdant_preview();")
    idle.main()


if __name__ == "__main__":
    main()

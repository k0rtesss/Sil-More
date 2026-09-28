#!/usr/bin/env python3
"""Exercise cosmetic ordinary-cave palettes through production C and SDL.

Build with build-incremental.ps1 first. Tests use isolated cave grids and never
open a player save. The output contact sheet uses real ordinary CA generation,
real style templates, map_info(), and the production SDL terrain compositor.
"""

import check_cave_floor_tiles as floors
import check_idle_animation as idle


TESTS = r'''
void patch_generation_reset(void);
void patch_generation_snapshot(void);
bool patch_generation_unchanged(void);
bool carve_ca_blob_anchor_bounds(int,int,int,int,int);
void cave_apply_floor_palette(int,int,int,int,int);

typedef struct patch_cell {
    byte feat, color, natural, fixture, rewired;
    u16b info;
    s16b light, monster, object;
} patch_cell;
typedef struct patch_snapshot {
    patch_cell cell[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
} patch_snapshot;
static patch_snapshot patch_before, patch_after;
static const int patch_bases[4] = {52,54,42,43};
static cave_floor_palette patch_authored[4];

static void patch_capture(patch_snapshot* out)
{
    memset(out,0,sizeof(*out));
    for (int y=0;y<MAX_DUNGEON_HGT;y++)
        for (int x=0;x<MAX_DUNGEON_WID;x++) {
            patch_cell* c=&out->cell[y][x];
            c->feat=cave_feat[y][x]; c->color=cave_color[y][x];
            c->natural=cave_natural[y][x]; c->fixture=cave_fixture_at(y,x);
            c->rewired=cave_rewired[y][x]; c->info=cave_info[y][x];
            c->light=cave_light[y][x]; c->monster=cave_m_idx[y][x];
            c->object=cave_o_idx[y][x];
        }
}

static void patch_reset(int base)
{
    cave_fixtures_clear(); sdl_idle_animation_clear_cells();
    use_bigtile=false; p_ptr->wx=p_ptr->wy=0;
    p_ptr->cur_map_hgt=p_ptr->cur_map_wid=32;
    p_ptr->blind=p_ptr->rage=p_ptr->image=0; p_ptr->is_dead=false;
    p_ptr->py=p_ptr->px=30; p_ptr->depth=8;
    g_labyrinth_view_active=false; g_state.use_tiles=true;
    patch_generation_reset();
    for (int y=0;y<MAX_DUNGEON_HGT;y++)
        for (int x=0;x<MAX_DUNGEON_WID;x++) {
            cave_feat[y][x]=FEAT_WALL_EXTRA;
            cave_color[y][x]=(byte)(COLOR_STYLE_BASE+base);
            cave_info[y][x]=CAVE_WALL;
            cave_natural[y][x]=cave_rewired[y][x]=0;
            cave_light[y][x]=2;
            cave_m_idx[y][x]=cave_o_idx[y][x]=0;
        }
}

static void patch_floor(int y,int x,int base)
{
    cave_feat[y][x]=FEAT_FLOOR;
    cave_info[y][x]=CAVE_ROOM|CAVE_MARK|CAVE_SEEN|CAVE_GLOW;
    cave_color[y][x]=(byte)(COLOR_STYLE_BASE+base);
    cave_natural[y][x]=1;
}

static void patch_rectangle(int y1,int y2,int x1,int x2,int base)
{
    for (int y=y1;y<=y2;y++) for (int x=x1;x<=x2;x++) patch_floor(y,x,base);
}

static bool patch_eligible(const patch_cell* c,int base)
{
    return c->feat==FEAT_FLOOR && (c->info&CAVE_ROOM) && c->natural
        && c->color==COLOR_STYLE_BASE+base
        && !(c->info&(CAVE_ICKY|CAVE_G_VAULT|CAVE_MORGOTH_TUNNEL|CAVE_CHASM_AREA))
        && !c->object && !c->monster && !c->fixture;
}

/* Independently validate changed cells, connected components and coverage. */
static int patch_validate(const patch_snapshot* before,int base,
    const cave_floor_palette* palette,int y1,int y2,int x1,int x2,bool compact)
{
    bool changed[32][32]={0},visited[32][32]={0},styles[64]={0};
    int eligible=0,count=0,components=0,distinct=0;
    for (int y=0;y<MAX_DUNGEON_HGT;y++)
        for (int x=0;x<MAX_DUNGEON_WID;x++) {
            const patch_cell* c=&before->cell[y][x];
            assert(cave_feat[y][x]==c->feat && cave_info[y][x]==c->info);
            assert(cave_natural[y][x]==c->natural && cave_light[y][x]==c->light);
            assert(cave_rewired[y][x]==c->rewired && cave_m_idx[y][x]==c->monster);
            assert(cave_o_idx[y][x]==c->object && cave_fixture_at(y,x)==c->fixture);
            bool allowed=y>=y1 && y<=y2 && x>=x1 && x<=x2
                && y<32 && x<32 && patch_eligible(c,base);
            if (allowed) eligible++;
            if (cave_color[y][x]==c->color) continue;
            assert(allowed);
            int style=styles_decode_color_style(cave_color[y][x]);
            assert(style>=0 && style<64 && style!=base);
            bool listed=false;
            for (int i=0;i<palette->count;i++) if (style==palette->styles[i]) listed=true;
            assert(listed);
            if (!styles[style]) { styles[style]=true; distinct++; }
            changed[y][x]=true; count++;
        }
    assert(count<=eligible*palette->coverage/100 && count*2<eligible+1);
    assert(distinct<=2);
    const int dy[4]={-1,1,0,0},dx[4]={0,0,-1,1};
    for (int y=0;y<32;y++) for (int x=0;x<32;x++) {
        if (!changed[y][x] || visited[y][x]) continue;
        int queue[1024],head=0,tail=0,min_y=y,max_y=y,min_x=x,max_x=x;
        byte color=cave_color[y][x]; queue[tail++]=y*32+x; visited[y][x]=true;
        while (head<tail) {
            int at=queue[head++],cy=at/32,cx=at%32;
            min_y=MIN(min_y,cy); max_y=MAX(max_y,cy);
            min_x=MIN(min_x,cx); max_x=MAX(max_x,cx);
            for (int i=0;i<4;i++) {
                int ny=cy+dy[i],nx=cx+dx[i];
                if (ny<0 || nx<0 || ny>=32 || nx>=32 || visited[ny][nx]
                    || !changed[ny][nx] || cave_color[ny][nx]!=color) continue;
                visited[ny][nx]=true; queue[tail++]=ny*32+nx;
            }
        }
        assert(tail>=4);
        if (compact) assert((max_y-min_y+1)*(max_x-min_x+1)<=tail*4);
        components++;
    }
    assert(components<=palette->patches);
    return count;
}

static void patch_apply_checked(int base,const cave_floor_palette* palette,
    int y1,int y2,int x1,int x2,bool compact)
{
    patch_capture(&patch_before);
    u64b rng=Rand_state_export(); s32b turns=turn,player_turns=playerturn;
    cave_apply_floor_palette(y1,y2,x1,x2,base);
    assert(Rand_state_export()==rng && turn==turns && playerturn==player_turns);
    patch_validate(&patch_before,base,palette,y1,y2,x1,x2,compact);
}

static void patch_authored_tests(void)
{
    unsigned source_reached=0;
    Rand_state_init(9137);
    for (int depth=1;depth<=20;depth++) for (int n=0;n<128;n++) {
        int style=styles_pick_partition_style(depth,PART_STYLE_CA_BLOB);
        for (int b=0;b<4;b++) if (style==patch_bases[b]) source_reached|=1u<<b;
    }
    assert(source_reached==15);
    for (int b=0;b<4;b++) {
        int base=patch_bases[b];
        assert(styles_cave_floor_palette(base,&patch_authored[b]));
        const cave_floor_palette* palette=&patch_authored[b];
        assert(palette->coverage>=20 && palette->coverage<=25);
        assert(palette->patches>=2 && palette->patches<=3);
        bool reached[64]={0};
        for (int seed=1;seed<=40;seed++) {
            patch_reset(base); patch_rectangle(4,25,4,25,base);
            Rand_state_init(1000+seed);
            patch_apply_checked(base,palette,2,29,2,29,true);
            int count=patch_validate(&patch_before,base,palette,2,29,2,29,true);
            assert(count>=4);
            patch_capture(&patch_after);
            for (int y=0;y<32;y++) for (int x=0;x<32;x++)
                if (cave_color[y][x]!=COLOR_STYLE_BASE+base)
                    reached[styles_decode_color_style(cave_color[y][x])]=true;
            patch_reset(base); patch_rectangle(4,25,4,25,base);
            Rand_state_init(1000+seed);
            cave_apply_floor_palette(2,29,2,29,base);
            patch_capture(&patch_before);
            assert(memcmp(&patch_before,&patch_after,sizeof(patch_before))==0);
        }
        int accents=0;
        for (int i=0;i<palette->count;i++) if (reached[palette->styles[i]]) accents++;
        assert(accents>0);
        printf("Authored base %d: 40 deterministic seeds, compact connected patches, bounded coverage and %d reachable accents: PASS\n",base,accents);
    }
}

static void patch_protection_tests(void)
{
    int base=52;
    const cave_floor_palette* palette=&patch_authored[0];
    for (int seed=0;seed<16;seed++) {
        patch_reset(base); patch_rectangle(3,28,3,28,base);
        const u16b flags[]={CAVE_ICKY,CAVE_G_VAULT,CAVE_MORGOTH_TUNNEL,CAVE_CHASM_AREA};
        for (int i=0;i<4;i++) cave_info[9][7+i]|=flags[i];
        cave_natural[10][7]=0; cave_info[10][8]&=~CAVE_ROOM;
        cave_color[10][9]=(byte)(COLOR_STYLE_BASE+base+COLOR_STYLE_FLAG_FIRSTVAR);
        cave_color[10][10]=(byte)(COLOR_STYLE_BASE+54);
        cave_o_idx[11][7]=1; cave_m_idx[11][8]=1; cave_m_idx[11][9]=-1;
        cave_feat[11][10]=FEAT_WALL_EXTRA;
        cave_fixture_set(11,10,CAVE_FIXTURE_BRAZIER);
        assert(cave_fixture_at(11,10)==CAVE_FIXTURE_BRAZIER);
        const byte feats[]={FEAT_WALL_EXTRA,FEAT_WATER,FEAT_LAVA,FEAT_ICE,
            FEAT_POISON,FEAT_CHASM,FEAT_RUBBLE,FEAT_LESS,FEAT_TRAP_PIT,FEAT_OPEN};
        for (unsigned i=0;i<sizeof(feats);i++) cave_feat[12][5+i]=feats[i];
        Rand_state_init(2000+seed);
        patch_apply_checked(base,palette,5,25,5,25,false);
    }
    patch_reset(base); patch_rectangle(1,30,1,30,base);
    Rand_state_init(3001);
    patch_apply_checked(base,palette,-100,999,-100,999,true);
    patch_capture(&patch_before);
    cave_apply_floor_palette(20,10,20,10,base);
    patch_capture(&patch_after);
    assert(memcmp(&patch_before,&patch_after,sizeof(patch_before))==0);
    puts("Vaults, authored cells, fixtures, occupants, hazards and bounds remain protected: PASS");
}

static void patch_fragment_tests(void)
{
    int base=52; const cave_floor_palette* palette=&patch_authored[0];
    patch_reset(base); patch_floor(7,7,base); patch_floor(7,8,base); patch_floor(8,8,base);
    patch_apply_checked(base,palette,1,30,1,30,false);
    for (int y=0;y<32;y++) for (int x=0;x<32;x++) assert(cave_color[y][x]==COLOR_STYLE_BASE+base);
    patch_reset(base);
    for (int y=3;y<29;y+=3) for (int x=3;x<29;x+=3) patch_floor(y,x,base);
    patch_apply_checked(base,palette,1,30,1,30,false);
    for (int y=0;y<32;y++) for (int x=0;x<32;x++) assert(cave_color[y][x]==COLOR_STYLE_BASE+base);
    patch_reset(base);
    patch_rectangle(4,10,4,10,base); patch_rectangle(18,24,18,24,base);
    patch_floor(14,14,base); patch_floor(14,15,base);
    patch_apply_checked(base,palette,1,30,1,30,false);
    assert(cave_color[14][14]==COLOR_STYLE_BASE+base && cave_color[14][15]==COLOR_STYLE_BASE+base);
    puts("Tiny and isolated floor fragments are skipped; disconnected rooms keep valid components: PASS");
}

static void patch_equivalent_tests(void)
{
    for (int pass=0;pass<2;pass++) {
        int b=pass?2:0,base=patch_bases[b],alias=43;
        style_type saved=style_info[alias];
        style_info[alias]=style_info[base];
        if (!pass) {
            int choice=cave_style_floor_choice(base);
            style_info[alias].floor_rowv[0]=style_info[base].floor_rowv[choice];
            style_info[alias].floor_colv[0]=style_info[base].floor_colv[choice];
            style_info[alias].floor_count=1;
        } else if (style_info[alias].floor_count>1) {
            int last=style_info[alias].floor_count-1;
            byte row=style_info[alias].floor_rowv[0],col=style_info[alias].floor_colv[0];
            style_info[alias].floor_rowv[0]=style_info[alias].floor_rowv[last];
            style_info[alias].floor_colv[0]=style_info[alias].floor_colv[last];
            style_info[alias].floor_rowv[last]=row; style_info[alias].floor_colv[last]=col;
        }
        cave_floor_palette same={25,3,1,{alias},{10}};
        assert(styles_set_cave_floor_palette(base,&same));
        patch_reset(base); patch_rectangle(3,28,3,28,base);
        patch_capture(&patch_before);
        cave_apply_floor_palette(1,30,1,30,base);
        patch_capture(&patch_after);
        assert(memcmp(&patch_before,&patch_after,sizeof(patch_before))==0);
        assert(styles_set_cave_floor_palette(base,&patch_authored[b]));
        style_info[alias]=saved;
    }
    puts("Equivalent selected original tiles and reordered Verdant families do not create false accents: PASS");
}

static void patch_weight_tests(void)
{
    cave_floor_palette weighted={25,2,3,{41,42,43},{1000,1,1}};
    assert(styles_set_cave_floor_palette(52,&weighted));
    int heavy=0,light_a=0,light_b=0;
    for (unsigned seed=1;seed<=64;seed++) {
        patch_reset(52); patch_rectangle(4,25,4,25,52);
        Rand_state_init(5000+seed);
        patch_apply_checked(52,&weighted,1,30,1,30,true);
        bool used[64]={0};
        for (int y=0;y<32;y++) for (int x=0;x<32;x++)
            used[styles_decode_color_style(cave_color[y][x])]=true;
        assert((int)used[41]+used[42]+used[43]==2);
        heavy+=used[41]; light_a+=used[42]; light_b+=used[43];
    }
    assert(heavy>=60 && light_a>0 && light_b>0);
    assert(styles_set_cave_floor_palette(52,&patch_authored[0]));
    puts("Weighted selection favours configured accents and selects without replacement: PASS");
}

static void patch_carve(int base,unsigned seed,bool enabled)
{
    patch_reset(base);
    styles_cave_floor_palettes_clear();
    if (enabled) for (int i=0;i<4;i++) assert(styles_set_cave_floor_palette(patch_bases[i],&patch_authored[i]));
    Rand_state_init(seed);
    assert(carve_ca_blob_anchor_bounds(2,28,2,28,base));
}

static void patch_integration_tests(void)
{
    for (int b=0;b<4;b++) {
        int changed=0;
        for (unsigned seed=4000;seed<4008;seed++) {
            patch_carve(patch_bases[b],seed,false);
            patch_capture(&patch_before);
            u64b rng=Rand_state_export(); patch_generation_snapshot();
            patch_carve(patch_bases[b],seed,true);
            assert(Rand_state_export()==rng);
            assert(patch_generation_unchanged());
            changed+=patch_validate(&patch_before,patch_bases[b],&patch_authored[b],0,31,0,31,false);
        }
        assert(changed>0);
    }
    puts("Ordinary CA carving reaches original and Verdant palettes without changing geometry, anchors or gameplay RNG: PASS");
}

static void patch_label(TTF_Font* font,int x,int y,const char* label)
{
    SDL_Color color={235,236,224,255};
    SDL_Surface* text=TTF_RenderText_Blended(font,label,0,color); assert(text);
    SDL_Texture* texture=SDL_CreateTextureFromSurface(g_state.renderer,text); assert(texture);
    SDL_FRect dst={(float)x,(float)y,(float)text->w,(float)text->h};
    SDL_DestroySurface(text);
    SDL_RenderTexture(g_state.renderer,texture,NULL,&dst); SDL_DestroyTexture(texture);
}

static void patch_preview(void)
{
    const int width=28,height=28,panel_w=width*TILE_SIZE,panel_h=22+height*TILE_SIZE;
    SDL_Texture* previous=SDL_GetRenderTarget(g_state.renderer);
    SDL_Texture* target=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,panel_w*2,panel_h*4);
    TTF_Font* font=TTF_OpenFont("lib/xtra/font/VictorMono-Medium.ttf",13);
    assert(target && font); SDL_SetRenderTarget(g_state.renderer,target);
    SDL_SetRenderDrawColor(g_state.renderer,7,9,12,255); SDL_RenderClear(g_state.renderer);
    for (int b=0;b<4;b++) for (int after=0;after<2;after++) {
        patch_carve(patch_bases[b],4096+b,after!=0);
        char label[96];
        strnfmt(label,sizeof(label),"%s %d / %s",b<2?"Original":"Verdant",patch_bases[b],after?"palette patches":"base floor");
        patch_label(font,after*panel_w+5,b*panel_h+3,label);
        for (int y=0;y<32;y++) for (int x=0;x<32;x++)
            cave_info[y][x]|=CAVE_MARK|CAVE_SEEN|CAVE_GLOW;
        for (int y=0;y<height;y++) for (int x=0;x<width;x++) {
            byte a,ta; char c,tc;
            SDL_FRect dst={(float)(after*panel_w+x*TILE_SIZE),
                (float)(b*panel_h+22+y*TILE_SIZE),TILE_SIZE,TILE_SIZE};
            map_info(y+2,x+2,&a,&c,&ta,&tc);
            sdl_draw_map_tile_layers_at(y+2,x+2,a,c,ta,tc,&dst);
        }
    }
    SDL_Surface* image=SDL_RenderReadPixels(g_state.renderer,NULL); assert(image);
    assert(IMG_SavePNG(image,"scripts/output/cave-floor-patches-check/before-after.png"));
    for (int b=0;b<4;b++) {
        SDL_Surface* row=SDL_CreateSurface(panel_w*2,panel_h,image->format); assert(row);
        SDL_Rect region={0,b*panel_h,panel_w*2,panel_h};
        assert(SDL_BlitSurface(image,&region,row,NULL));
        char path[160];
        strnfmt(path,sizeof(path),"scripts/output/cave-floor-patches-check/style-%d-before-after.png",patch_bases[b]);
        assert(IMG_SavePNG(row,path)); SDL_DestroySurface(row);
    }
    SDL_DestroySurface(image); TTF_CloseFont(font);
    SDL_SetRenderTarget(g_state.renderer,previous); SDL_DestroyTexture(target);
    puts("Production SDL before/after ordinary-cave palettes for original52/54 and Verdant42/43: PASS");
}

static void patch_tests(void)
{
    if (!cave_natural) cave_natural=calloc(MAX_DUNGEON_HGT,sizeof(*cave_natural));
    assert(cave_natural);
    /* The shared idle fixture seeds floor and wall visuals only. Ordinary
     * CA generation also places quartz, so load its authored terrain sprite
     * rather than rendering the fixture's otherwise empty tile coordinate. */
    FILE* terrain=fopen("lib/edit/terrain.txt","r"); assert(terrain);
    char line[1024]; int feature=-1; bool quartz_loaded=false;
    while (fgets(line,sizeof(line),terrain)) {
        if (line[0]=='N') { assert(sscanf(line,"N:%d:",&feature)==1); continue; }
        int row,col;
        if (feature==FEAT_QUARTZ && sscanf(line,"T:%d:%d",&row,&col)==2) {
            f_info[FEAT_QUARTZ].x_attr=(byte)(TILE_FLAG|row);
            f_info[FEAT_QUARTZ].x_char=(char)(TILE_FLAG|col);
            quartz_loaded=true;
        }
    }
    fclose(terrain); assert(quartz_loaded);
    patch_authored_tests(); patch_protection_tests(); patch_fragment_tests();
    patch_equivalent_tests(); patch_weight_tests(); patch_integration_tests(); patch_preview();
}
'''

GENERATION_FIXTURE = r'''
#include "level-generation/level-generation-internal.h"
static dun_data patch_dungeon,patch_original;
void patch_generation_reset(void) {
    memset(&patch_dungeon,0,sizeof(patch_dungeon)); dun=&patch_dungeon;
}
void patch_generation_snapshot(void) { patch_original=*dun; }
bool patch_generation_unchanged(void) {
    return memcmp(&patch_original,dun,sizeof(patch_original))==0;
}
'''


def main():
    idle.OUT = idle.ROOT / "scripts/output/cave-floor-patches-check"
    idle.HARNESS = idle.HARNESS.replace(
        "scripts/output/idle-animation-check", "scripts/output/cave-floor-patches-check")
    idle.HARNESS = idle.HARNESS.replace(
        "int main(void) {", floors.TESTS + TESTS + "\nint main(void) {")
    idle.HARNESS = idle.HARNESS.replace(
        "    asynchronous_tests();",
        "    asynchronous_tests();\n    floor_templates();\n    patch_tests();")
    # externs.h has enum declarations and no include guard. Keep generation
    # internals separate from the SDL header rather than duplicating structs.
    idle.OUT.mkdir(parents=True, exist_ok=True)
    fixture = idle.OUT / "generation-fixture.c"
    fixture.write_text(GENERATION_FIXTURE, encoding="utf-8")
    run = idle.subprocess.run

    def with_generation_fixture(command, *args, **kwargs):
        if str(command[0]).endswith("cc.exe"):
            command.insert(command.index("@CMakeFiles/sil-more.dir/linkLibs.rsp"), str(fixture))
        return run(command, *args, **kwargs)

    idle.subprocess.run = with_generation_fixture
    try:
        idle.main()
    finally:
        idle.subprocess.run = run


if __name__ == "__main__":
    main()

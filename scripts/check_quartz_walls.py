#!/usr/bin/env python3
"""Production-engine regression for mineral veins, damaged walls and SDL art.

Build standard first. Uses temporary template caches, never player saves.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
from check_living_dungeon_save import ENGINE_FIXTURE, fixture_function, FRESH_MAP
from check_monster_scent_save import WRITER, READER

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build-standard'
OUT = ROOT / 'scripts/output/quartz-walls'

TESTS = r'''
int checks;
static int rolls, result, gems;
static bool real_gems;
bool __real_drop_generate_object_profiled(int,drop_quality,int,int,bool,const drop_profile*,object_type*);
bool test_tunnel(int y,int x);
#define CHECK(t) do { checks++; if(!(t)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#t);exit(1);} }while(0)
int __wrap_skill_check(monster_type* a,int score,int difficulty,monster_type* b)
{ (void)a;(void)score;(void)difficulty;(void)b;rolls++;return result; }
bool __wrap_drop_generate_object_profiled(int depth,drop_quality quality,
    int type,int bonus,bool chest,const drop_profile* profile,object_type* out)
{
    (void)depth;(void)quality;(void)type;(void)bonus;(void)chest;(void)out;
    CHECK(profile->supply_gem>0 && !profile->supply_staff);gems++;
    if(real_gems)return __real_drop_generate_object_profiled(depth,quality,type,bonus,chest,profile,out);
    return false; /* Exercise guaranteed shallow fallback with an empty pool. */
}
void clean(int mode,int depth)
{
    fresh_map();turn=1000;p_ptr->depth=depth;character_dungeon=true;
    partition_meta_save meta={0};meta.grid_rows=meta.grid_cols=meta.partition_count=1;
    meta.modes[0]=mode;level_partition_meta_set(&meta);
    rolls=gems=0;result=1;memset(inventory,0,sizeof(inventory));
    terrain_vault_reset();
}
void stone(int feat)
{
    character_dungeon=false;cave_set_feat(10,10,feat);character_dungeon=true;
    cave_info[10][10]|=CAVE_MARK;
}
static int count_kind(int tval,int sval)
{
    int n=0;for(int i=1;i<o_max;i++)if(o_list[i].k_idx && o_list[i].tval==tval
        && (sval<0||o_list[i].sval==sval))n+=o_list[i].number;return n;
}
static void test_damage(void)
{
    const int walls[]={FEAT_WALL_EXTRA,FEAT_QUARTZ,FEAT_DAMAGED_WALL,FEAT_CRACKED_QUARTZ};
    for(int i=0;i<4;i++) {
        clean(QUAD_MODE_ROOMY,1);stone(walls[i]);result=0;
        project_f(-1,10,10,0,8,4,14,GF_KILL_WALL);
        CHECK(rolls==1 && cave_feat[10][10]==walls[i] && gems==0);
        result=1;project_f(-1,10,10,0,8,4,14,GF_KILL_WALL);
        int expected=i==0?FEAT_DAMAGED_WALL:i==1?FEAT_CRACKED_QUARTZ:FEAT_RUBBLE;
        CHECK(cave_feat[10][10]==expected && cave_wall_bold(10,10));
        CHECK(!terrain_generation_walkable(10,10,NULL));
        if(i<2) {
            CHECK(gems==0);project_f(-1,10,10,0,8,4,14,GF_KILL_WALL);
            CHECK(cave_feat[10][10]==FEAT_RUBBLE);
        }
        CHECK(gems==(i==1||i==3));
        project_f(-1,10,10,0,8,4,14,GF_KILL_WALL);
        CHECK(cave_feat[10][10]==FEAT_FLOOR && terrain_generation_walkable(10,10,NULL));
        CHECK(count_kind(TV_GEM,-1)==(i==1||i==3));
    }
    clean(QUAD_MODE_ROOMY,1);stone(FEAT_WALL_EXTRA);result=5;
    project_f(-1,10,10,0,8,4,14,GF_KILL_WALL);
    CHECK(cave_feat[10][10]==FEAT_RUBBLE && rolls==1);
    stone(FEAT_WALL_PERM);rolls=0;project_f(-1,10,10,0,8,4,99,GF_KILL_WALL);
    CHECK(cave_feat[10][10]==FEAT_WALL_PERM && rolls==0);
    puts("Blasting: one check, staged/strong outcomes, permanent walls, once-only gems PASS");
}
static void test_digging(void)
{
    clean(QUAD_MODE_ROOMY,1);
    int kind=0;for(int i=1;i<z_info->k_max;i++)if(k_info[i].flags1&TR1_TUNNEL){kind=i;break;}
    CHECK(kind>0);object_prep(&inventory[INVEN_WIELD],kind);
    inventory[INVEN_WIELD].pval=3;object_known(&inventory[INVEN_WIELD]);
    p_ptr->stat_use[A_STR]=2;stone(FEAT_WALL_EXTRA);
    test_tunnel(10,10);CHECK(cave_feat[10][10]==FEAT_WALL_EXTRA && rolls==0);
    p_ptr->stat_use[A_STR]=3;result=-5;test_tunnel(10,10);
    CHECK(cave_feat[10][10]==FEAT_DAMAGED_WALL && rolls==1);
    test_tunnel(10,10);CHECK(cave_feat[10][10]==FEAT_RUBBLE);
    stone(FEAT_QUARTZ);result=5;test_tunnel(10,10);
    CHECK(cave_feat[10][10]==FEAT_RUBBLE && gems==1);
    test_tunnel(10,10);CHECK(cave_feat[10][10]==FEAT_FLOOR && gems==1);
    puts("Digging: tool/Strength gates, guaranteed progress, strong collapse PASS");
}
static void test_minerals(void)
{
    const int modes[]={QUAD_MODE_ROOMY,QUAD_MODE_CAVEY,QUAD_MODE_BIG_CAVE,QUAD_MODE_RUINED,QUAD_MODE_CHASM};
    for(int m=0;m<5;m++)for(int depth=1;depth<=23;depth++) {
        clean(modes[m],depth);cave_info[10][10]|=CAVE_CHASM_AREA;
        int ore=cave_quartz_metal_kind(10,10,depth);
        CHECK(ore==(modes[m]==QUAD_MODE_CHASM?(depth>=10?SV_METAL_STAR_IRON:-1)
            :(depth>=12?SV_METAL_MITHRIL:-1)));
        stone(FEAT_QUARTZ);cave_set_feat(10,10,FEAT_CRACKED_QUARTZ);CHECK(gems==0);
        cave_set_feat(10,10,FEAT_RUBBLE);CHECK(gems==1 && count_kind(TV_GEM,-1)==1);
        CHECK(modes[m]==QUAD_MODE_CHASM || count_kind(TV_METAL,SV_METAL_STAR_IRON)==0);
        cave_set_feat(10,10,FEAT_FLOOR);CHECK(gems==1);
    }
    /* Exercise the real depth-weighted generator as well as its empty-pool fallback. */
    real_gems=true;drop_system_init();
    for(int depth=1;depth<=20;depth+=3){
        clean(QUAD_MODE_CAVEY,depth);stone(FEAT_QUARTZ);
        cave_set_feat(10,10,FEAT_RUBBLE);CHECK(count_kind(TV_GEM,-1)==1);
    }
    real_gems=false;
    puts("Rewards: every depth/location, real gem generation/fallback, crater-only metals, no duplicates PASS");
}
static void test_generation(void)
{
    for(int mode=0;mode<=QUAD_MODE_BIG_CAVE;mode++) {
        clean(mode,12);character_dungeon=false;
        for(int y=6;y<14;y++)for(int x=6;x<18;x++){
            cave_set_feat(y,x,FEAT_WALL_EXTRA);
            cave_info[y-1][x]|=CAVE_ROOM;cave_natural[y-1][x]=1;
        }
        /* A natural floor border and one protected architectural wall. */
        for(int x=6;x<18;x++){cave_set_feat(5,x,FEAT_FLOOR);cave_info[5][x]|=CAVE_ROOM;cave_natural[5][x]=1;}
        cave_info[6][8]|=CAVE_ICKY;
        for(int i=0;i<100;i++)scatter_quartz_veins_in_bounds(6,13,6,17,0);
        CHECK(cave_feat[6][8]==FEAT_WALL_EXTRA);
        int n=0;for(int x=6;x<18;x++)n+=cave_feat[6][x]==FEAT_QUARTZ;
        CHECK((mode==QUAD_MODE_CAVEY||mode==QUAD_MODE_BIG_CAVE||mode==QUAD_MODE_CHASM)?n>0:n==0);
    }
    clean(QUAD_MODE_CAVEY,12);character_dungeon=false;
    cave_set_feat(10,10,FEAT_WALL_OUTER);cave_info[9][10]|=CAVE_ROOM;
    /* A constructed room inside a cave partition is not a natural exposure. */
    for(int n=0;n<100;n++)scatter_quartz_veins_in_bounds(9,11,9,11,0);
    CHECK(cave_feat[10][10]==FEAT_WALL_OUTER);
    /* Explicit templates retain minerals only in the selected mine/caverns. */
    for(int v=0;v<z_info->v_max;v++)if(v_info[v].name) {
        const char* data=v_text+v_info[v].text;
        int length=v_info[v].hgt*v_info[v].wid;
        for(int j=0;j<length;j++)if(data[j]=='%')CHECK(v==59||v==365||v==366);
    }
    puts("Generation: natural partitions, protected construction, explicit vault deposits PASS");
}
static void test_environment(void)
{
    clean(QUAD_MODE_CAVEY,12);stone(FEAT_WALL_EXTRA);cave_natural[10][10]=1;
    cave_set_feat(10,9,FEAT_POISON);cave_environment_seed();
    environment_cell cell=*cave_environment_cell_at(10,10);
    cell.integrity=69;CHECK(cave_environment_restore_cell(10,10,cell));
    bool damaged=false,rubble=false;
    for(int i=0;i<100;i++){
        for(int j=0;j<cave_environment_get_state().source_count;j++){
            environment_source source=*cave_environment_source_at(j);
            source.next_turn=turn;source.phase=0;CHECK(cave_environment_restore_source(j,source));
        }
        turn+=100;cave_environment_process();
        if(!damaged && cave_feat[10][10]==FEAT_DAMAGED_WALL){
            damaged=true;cell=*cave_environment_cell_at(10,10);cell.integrity=25;
            CHECK(cave_environment_restore_cell(10,10,cell));
        }
        if(cave_feat[10][10]==FEAT_RUBBLE){rubble=true;break;}
    }
    CHECK(damaged&&rubble&&gems==0);
    clean(QUAD_MODE_RUINED,12);stone(FEAT_WALL_EXTRA);cave_natural[10][10]=1;
    cave_set_feat(10,11,FEAT_QUARTZ);cave_environment_seed();
    for(int i=0;i<2000;i++){turn+=100;cave_environment_process();}
    CHECK(cave_feat[10][10]==FEAT_WALL_EXTRA&&cave_environment_get_state().mineral_budget==8);
    puts("Environment: visible damage before erosion collapse; ruins cannot grow quartz PASS");
}
size_t fixture_write_dungeon(byte*,size_t,size_t*);
int fixture_read_dungeon(const byte*,size_t,int,u32b*,size_t*);
static void test_save(void)
{
    static byte buffer[262144];size_t dungeon,length,consumed;u32b sentinel;
    clean(QUAD_MODE_CAVEY,12);stone(FEAT_CRACKED_QUARTZ);
    cave_set_feat(10,11,FEAT_DAMAGED_WALL);cave_environment_seed();
    cave_event_emit(CAVE_EVENT_MINERAL,10,10,8);
    length=fixture_write_dungeon(buffer,sizeof(buffer),&dungeon);
    fresh_map();turn=1000;
    CHECK(!fixture_read_dungeon(buffer,length,VERSION_EXTRA,&sentinel,&consumed));
    CHECK(consumed==length&&sentinel==0xA1B2C3D4U);
    CHECK(cave_feat[10][10]==FEAT_CRACKED_QUARTZ&&cave_feat[10][11]==FEAT_DAMAGED_WALL);
    CHECK(cave_wall_bold(10,10)&&!cave_floor_bold(10,11));
    CHECK(cave_environment_cell_at(10,10)->known_feat==FEAT_CRACKED_QUARTZ);
    cave_world_event event;CHECK(cave_event_latest(&event)&&event.kind==CAVE_EVENT_MINERAL);
    for(int mode=QUAD_MODE_ROOMY;mode<=QUAD_MODE_CAVEY;mode++){
        clean(mode,12);stone(FEAT_QUARTZ);cave_set_feat(10,11,FEAT_QUARTZ);
        cave_info[10][11]|=CAVE_ICKY;cave_environment_seed();
        length=fixture_write_dungeon(buffer,sizeof(buffer),&dungeon);
        /* Partition metadata lives in the player header, outside this dungeon fixture. */
        clean(mode,12);character_dungeon=false;
        CHECK(!fixture_read_dungeon(buffer,length,21,&sentinel,&consumed));
        CHECK(cave_feat[10][11]==FEAT_DAMAGED_WALL);
        CHECK(cave_feat[10][10]==(mode==QUAD_MODE_CAVEY?FEAT_QUARTZ:FEAT_DAMAGED_WALL));
        CHECK(cave_environment_cell_at(10,11)->known_feat==FEAT_DAMAGED_WALL);
        CHECK(o_cnt==0);
    }
    puts("Save/load: new terrain/event roundtrip and conservative v21 migration without rewards PASS");
}
static void test_visual(void)
{
    visual_clean();use_graphics=GRAPHICS_MICROCHASM;g_state.use_tiles=true;
    SDL_Surface* source=IMG_Load(TEST_ROOT "/lib/xtra/graf/16x16.png");CHECK(source);
    SDL_Surface* atlas=sdl_quartz_tileset_surface(source);CHECK(atlas);
    CHECK(GRAPHICS_QUARTZ_OVERLAY_ROW<=TILE_INDEX_MASK);
    extern byte get_default_vein_row(void);extern byte get_default_vein_col(void);
    for(int y=0;y<16;y++)for(int x=0;x<16;x++){
        Uint8 r,g,b,a,qr,qg,qb,qa;
        SDL_ReadSurfacePixel(source,get_default_vein_col()*16+x,get_default_vein_row()*16+y,&r,&g,&b,&a);
        SDL_ReadSurfacePixel(atlas,x,GRAPHICS_QUARTZ_OVERLAY_ROW*16+y,&qr,&qg,&qb,&qa);
        CHECK(a==qa);if(a)CHECK(qr>=100 && qg>=qr && qb>=qg && qb-qr<=20 && qb<220);
    }
    SDL_Surface* preview=SDL_CreateSurface(512,256,SDL_PIXELFORMAT_RGBA32);CHECK(preview);
    g_state.renderer=SDL_CreateSoftwareRenderer(preview);CHECK(g_state.renderer);
    g_state.tileset=SDL_CreateTextureFromSurface(g_state.renderer,atlas);CHECK(g_state.tileset);
    SDL_SetTextureBlendMode(g_state.tileset,SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(g_state.tileset,SDL_SCALEMODE_NEAREST);
    SDL_SetRenderDrawColor(g_state.renderer,20,20,24,255);SDL_RenderClear(g_state.renderer);
    const int features[]={FEAT_WALL_EXTRA,FEAT_DAMAGED_WALL,FEAT_QUARTZ,FEAT_CRACKED_QUARTZ};
    for(int row=0;row<2;row++)for(int col=0;col<4;col++){
        stone(features[col]);cave_color[10][10]=COLOR_STYLE_BASE+1;
        cave_info[10][10]|=CAVE_MARK|CAVE_SEEN;cave_light[10][10]=row?0:2;
        byte a,ta;char c,tc;map_info(10,10,&a,&c,&ta,&tc);
        if(col>=2)CHECK((a&TILE_INDEX_MASK)==GRAPHICS_QUARTZ_OVERLAY_ROW);
        else CHECK((a&TILE_INDEX_MASK)!=GRAPHICS_QUARTZ_OVERLAY_ROW);
        SDL_FRect dst={col*128.0f+16,row*128.0f+16,96,96};
        sdl_draw_tileset_sprite(ta,tc,&dst,false);sdl_draw_tileset_sprite(a,c,&dst,false);
    }
    SDL_RenderPresent(g_state.renderer);CHECK(IMG_SavePNG(preview,TEST_OUT "/quartz-preview.png"));
    SDL_DestroyTexture(g_state.tileset);g_state.tileset=NULL;
    SDL_DestroyRenderer(g_state.renderer);g_state.renderer=NULL;
    SDL_DestroySurface(preview);SDL_DestroySurface(atlas);SDL_DestroySurface(source);
    puts("SDL: preserved alpha mask, cool silver-white mineral palette, light/dark map rendering PASS");
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]
    for header in ('cave/cave-fixtures.h', 'cave/cave-flood.h',
                   'cave/cave-water-flow.h', 'cave/cave-environment.h', 'cave/cave-events.h',
                   'level-generation/level-generation-internal.h',
                   'level-generation/level-generation-terrain-access.h',
                   'level-generation/level-generation-terrain-vaults.h',
                   'monster/monster-senses.h', 'spell/spell-projection-internal.h'):
        prefix += f'\n#include "{header}"\n'
    prefix += f'#define TEST_ROOT "{ROOT.as_posix()}"\n#define TEST_OUT "{OUT.as_posix()}"\n'
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init = init[:init.index('    check_templates();')] + 'assert(init_flavor_info()==0);flavor_init();style_info=style_head.info_ptr;style_name=style_head.name_ptr;\n'
    visual = OUT / 'visual.c'
    split = TESTS.index('static void test_visual(void)')
    check_macro = TESTS[TESTS.index('#define CHECK'):TESTS.index('int __wrap_skill_check')]
    paths = f'#define TEST_ROOT "{ROOT.as_posix()}"\n#define TEST_OUT "{OUT.as_posix()}"\n'
    visual.write_text('#include "sdl/main-sdl-private.h"\n#include "cave/cave-environment.h"\n#include <stdio.h>\n#include <stdlib.h>\n'
                      'extern int checks;void visual_clean(void);void stone(int);\n' + paths + check_macro +
                      TESTS[split:].replace('static void test_visual(void)', 'void test_visual(void)'), encoding='utf-8')
    tests = TESTS[:split] + '\nvoid visual_clean(void){clean(QUAD_MODE_CAVEY,12); }\nvoid test_visual(void);\n'
    source = OUT / 'check.c' 
    source.write_text(prefix + fixture_function('terminal_extra') + '\n' +
                      fixture_function('reset_map') + '\n' + FRESH_MAP + '\n' + tests + '\n' + init +
                      'test_damage();test_digging();test_minerals();test_generation();test_environment();test_save();test_visual();\n'
                      'printf("Quartz walls: %d checks PASS.\\n",checks);SDL_Quit();return 0;}\n', encoding='utf-8')
    interaction = OUT / 'interact.c'
    interaction.write_text('#include "cmd/world/cmd-interact.c"\n'
                           'bool test_tunnel(int y,int x){return do_cmd_tunnel_aux(y,x);}\n', encoding='utf-8')
    writer = OUT / 'writer.c'; writer.write_text(WRITER, encoding='utf-8')
    reader = OUT / 'reader.c'; reader.write_text(READER, encoding='utf-8')
    objects = shlex.split((BUILD / 'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    objects = [p for p in objects if not p.endswith(('/src/main.c.obj', '/src/cmd/world/cmd-interact.c.obj', '/src/fs/save.c.obj', '/src/fs/load.c.obj'))]
    response = OUT / 'objects.rsp'
    response.write_text('\n'.join('"' + p + '"' for p in objects), encoding='utf-8')
    env = os.environ.copy()
    env['PATH'] = os.pathsep.join([*(str(BUILD / '_deps' / n) for n in ('SDL', 'SDL_ttf', 'SDL_image', 'SDL_mixer')),
                                 'C:/msys64/mingw64/bin', 'C:/msys64/usr/bin', env['PATH']])
    exe = OUT / 'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe', '-DUSE_SDL', '-std=c17', '-O0', '-g',
                    '@CMakeFiles/sil-more.dir/includes_C.rsp', str(source), str(interaction), str(visual), str(writer), str(reader), '@' + str(response),
                    '@CMakeFiles/sil-more.dir/linkLibs.rsp', '-Wl,--wrap=skill_check',
                    '-Wl,--wrap=drop_generate_object_profiled', '-o', str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix='data-', dir=OUT) as data:
        subprocess.run([str(exe), str(ROOT / 'lib/edit'), data], cwd=data, env=env, check=True, timeout=120)


if __name__ == '__main__':
    main()

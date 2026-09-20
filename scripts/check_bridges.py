#!/usr/bin/env python3
"""Check encoded dry bridges using production movement, SDL pixels and save RLE.

Build first. All maps, byte streams and images are isolated under scripts/output.
"""
from pathlib import Path
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/bridge-check"

HARNESS = r'''
#include "angband.h"
#include "sdl/main-sdl-private.h"
#include "cave/cave-bridge.h"
#include "cave/cave-fixtures.h"
#include "cave/cave-environment.h"
#include "melee/melee-movement.h"
#include "melee/melee-util.h"
#include "level-generation/level-generation-terrain-access.h"
#include "rng.h"
extern int current_partition_count,current_partition_rows,current_partition_cols;
#include <assert.h>

void __wrap_handle_stuff(void) {}
void __wrap_perceive(void) {}
void __wrap_check_mandos_quest_interaction(void) {}
void __wrap_check_niena_quest_completion(void) {}
void __wrap_trigger_chasm_sanctum_ambush_if_needed(int y,int x) {(void)y;(void)x;}
void __wrap_update_mon(int m,bool full) {(void)m;(void)full;}
void __wrap_msg_print(cptr msg) {(void)msg;}
void __wrap_message_flush(void) {}
void __wrap_object_desc(char* buf,size_t size,const object_type* object,int pref,int mode) {
    (void)object;(void)pref;(void)mode;SDL_strlcpy(buf,"a ration",size);
}
static errr dummy_xtra(int n,int v) {(void)n;(void)v;return 0;}
static term test_term;
static const int materials[]={FEAT_WATER,FEAT_CHASM,FEAT_LAVA,FEAT_POISON,FEAT_ICE,FEAT_DEEP_WATER};

static void bridge_map(void) {
    character_dungeon=false;
    p_ptr->cur_map_hgt=p_ptr->cur_map_wid=32;
    p_ptr->py=p_ptr->px=10;p_ptr->wy=p_ptr->wx=0;
    p_ptr->chp=p_ptr->mhp=100;p_ptr->poisoned=p_ptr->diseased=0;
    p_ptr->blind=p_ptr->rage=p_ptr->leaping=p_ptr->truce=p_ptr->is_dead=false;
    p_ptr->playing=true;p_ptr->total_weight=0;
    p_ptr->resist_fire=p_ptr->resist_pois=1;p_ptr->oppose_fire=p_ptr->oppose_pois=0;
    p_ptr->update=p_ptr->redraw=p_ptr->window=0;
    g_labyrinth_view_active=false;
    current_partition_count=current_partition_rows=current_partition_cols=0;
    for(int y=0;y<32;y++)for(int x=0;x<32;x++) {
        cave_set_feat(y,x,FEAT_FLOOR);
        cave_info[y][x]=CAVE_MARK|CAVE_SEEN;
        cave_color[y][x]=COLOR_STYLE_BASE;cave_light[y][x]=2;
        cave_m_idx[y][x]=cave_o_idx[y][x]=cave_when[y][x]=0;
    }
    o_max=mon_max=1;o_cnt=mon_cnt=0;
    cave_fixtures_clear();sdl_idle_animation_clear_cells();
}

static void bridge_rules(void) {
    for(int material=0;material<6;material++)for(int axis=0;axis<2;axis++) {
        bridge_map();int feat=cave_bridge_feature(materials[material],axis);
        assert(feat==FEAT_BRIDGE_HEAD+material*2+axis);
        assert(cave_bridge_underlay(feat)==materials[material]);
        assert(cave_bridge_vertical(feat)==axis);
        cave_set_feat(10,11,feat);cave_set_feat(10,12,feat);
        assert(cave_floor_bold(10,11)&&cave_clean_bold(10,11)&&cave_empty_bold(10,11));
        assert(!cave_naked_bold(10,11)); /* Fixed feature builders must not erase a deck. */
        assert(terrain_generation_walkable(10,11,NULL));
        assert(water_movement_energy(100,FEAT_FLOOR,feat,false)==100);
        cave_m_idx[10][10]=-1;
        for(int step=0;step<3;step++) {
            p_ptr->energy_use=100;stealth_score=20;
            player_lava_begin_action();player_poison_terrain_begin_action();
            move_player(6);player_lava_end_action();player_poison_terrain_end_action();
            assert(p_ptr->px==11+step&&p_ptr->energy_use==100&&stealth_score==20);
            assert(p_ptr->chp==100&&!p_ptr->poisoned&&!p_ptr->diseased&&!p_ptr->leaping);
        }
        cave_m_idx[10][13]=0;p_ptr->px=11;cave_m_idx[10][11]=-1;
        update_smell();assert(cave_when[10][11]>0&&get_scent(10,11)>=0);
        p_ptr->energy_use=100;player_lava_begin_action();player_poison_terrain_begin_action();
        player_lava_end_action();player_poison_terrain_end_action();
        assert(p_ptr->chp==100&&!p_ptr->poisoned);
        assert(!cave_transform_elemental_terrain(10,11,GF_FIRE));
        assert(!cave_transform_elemental_terrain(10,11,GF_COLD));
        assert(cave_feat[10][11]==feat);

        cave_m_idx[10][11]=0;p_ptr->py=p_ptr->px=25;
        monster_type* m=&mon_list[1];memset(m,0,sizeof(*m));
        m->r_idx=1;m->fy=m->fx=10;m->hp=m->maxhp=100;m->alertness=ALERTNESS_ALERT;
        memset(&r_info[1],0,sizeof(r_info[1]));r_info[1].name=1;
        mon_max=2;cave_m_idx[10][10]=1;bool bash=false;
        assert(cave_exist_mon(&r_info[1],10,11,false,false));
        assert(cave_passable_mon(m,10,11,&bash)==100);
        assert(monster_terrain_penalty(m,10,11)==0);
        assert(monster_poison_step_damage(m,10,10,10,11)==0);
        process_move(m,10,11,false);
        assert(m->fx==11&&m->energy==0&&m->hp==100&&!m->poisoned);
        cave_m_idx[10][11]=0;mon_max=1;
        object_type object={0};object.k_idx=1;object.tval=TV_FOOD;object.number=1;
        object.pickup=true;
        int index=drop_near(&object,0,10,11);
        assert(index>0&&o_list[index].iy==10&&o_list[index].ix==11);
        assert(cave_feat[10][11]==feat);
        o_list[index].marked=true;
        byte a,ta;char c,tc;map_info(10,11,&a,&c,&ta,&tc);
        assert(a==object_attr(&o_list[index])&&(byte)c==(byte)object_char(&o_list[index]));
    }
    assert(cave_bridge_feature(FEAT_FLOOR,false)==FEAT_NONE);
    assert(!cave_feat_is_bridge(FEAT_POISON)&&!cave_feat_is_bridge(FEAT_BRIDGE_TAIL+1));
    puts("All12 bridge types: player/monster movement, wait, scent, dry contact, drops and object visibility PASS");
}

/* Insert the production feature RLE blocks, including their real reader guards. */
static byte bytes[200000];static int written,read_pos;
static u32b load_byte_offset,save_byte_offset;
static void wr_byte(byte b) {assert(written<(int)sizeof(bytes));bytes[written++]=b;save_byte_offset++;}
static void rd_byte(byte* b) {*b=read_pos<written?bytes[read_pos++]:0;load_byte_offset++;}
static void note(cptr s) {(void)s;assert(false);}
static void maybe_show_startup_loading_overlay(void) {}
@READ_GUARDS@
static void write_features(void) {
    int y,x;byte count,prev_char,tmp8u;
    @WRITE_RLE@
}
static int read_features(void) {
    int y,x,i;byte count,tmp8u;bool first_rle_pair;
    @READ_RLE@
    return 0;
}
@VERSION_POLICY@
static void bridge_save(void) {
    fixture_sf_major=VERSION_MAJOR;fixture_sf_minor=VERSION_MINOR;fixture_sf_patch=VERSION_PATCH;
    fixture_sf_extra=VERSION_EXTRA;assert(bridge_savefile_version_supported());
    assert(!bridge_savefile_version_at_most(VERSION_MAJOR,VERSION_MINOR,VERSION_PATCH,VERSION_EXTRA-1));
    fixture_sf_extra=VERSION_EXTRA-1;assert(bridge_savefile_version_supported());
    fixture_sf_extra=VERSION_EXTRA+1;assert(!bridge_savefile_version_supported());
    bridge_map();p_ptr->cur_map_hgt=32;p_ptr->cur_map_wid=64;
    for(int y=0;y<32;y++)for(int x=0;x<64;x++)
        cave_set_feat(y,x,y<8?FEAT_BRIDGE_WATER_H:FEAT_BRIDGE_HEAD+(y+x)%12);
    static byte original[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    memcpy(original,cave_feat,sizeof(original));written=read_pos=0;
    write_features();assert(written>0);
    memset(cave_feat,0,sizeof(original));assert(read_features()==0&&read_pos==written);
    for(int y=0;y<32;y++)for(int x=0;x<64;x++) {
        assert(cave_feat[y][x]==original[y][x]);assert(cave_floor_bold(y,x));
        assert(cave_bridge_underlay(cave_feat[y][x])==cave_bridge_underlay(original[y][x]));
    }
    /* Legacy ordinary terrain requires no migration or extra stream bytes. */
    for(int y=0;y<32;y++)for(int x=0;x<64;x++)
        cave_set_feat(y,x,(x%3)==0?FEAT_FLOOR:(x%3)==1?FEAT_WATER:FEAT_CHASM);
    memcpy(original,cave_feat,sizeof(original));written=read_pos=0;write_features();
    memset(cave_feat,0,sizeof(original));assert(read_features()==0&&read_pos==written);
    for(int y=0;y<32;y++)for(int x=0;x<64;x++)assert(cave_feat[y][x]==original[y][x]);
    puts("Production dungeon feature RLE: all bridge materials/axes, long runs and legacy terrain PASS");
}

static uint64_t render_cell(int feat,const char* path) {
    cave_set_feat(10,10,feat);cave_m_idx[10][10]=cave_o_idx[10][10]=0;
    byte a,ta;char c,tc;map_info(10,10,&a,&c,&ta,&tc);
    SDL_Texture* target=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,32,32);assert(target);
    SDL_SetRenderTarget(g_state.renderer,target);
    SDL_FRect dst={0,0,32,32};sdl_draw_map_tile_layers_at(10,10,a,c,ta,tc,&dst);
    SDL_Surface* surface=SDL_RenderReadPixels(g_state.renderer,NULL);assert(surface);
    SDL_Surface* rgba=SDL_ConvertSurface(surface,SDL_PIXELFORMAT_RGBA32);assert(rgba);
    uint64_t hash=1469598103934665603ULL;
    for(int y=0;y<32;y++)for(int x=0;x<128;x++)
        hash=(hash^*((byte*)rgba->pixels+y*rgba->pitch+x))*1099511628211ULL;
    if(path)assert(IMG_SavePNG(rgba,path));
    SDL_DestroySurface(rgba);SDL_DestroySurface(surface);
    SDL_SetRenderTarget(g_state.renderer,NULL);SDL_DestroyTexture(target);return hash;
}

static void bridge_environment_pixels(const char* out) {
    bridge_map();p_ptr->py=p_ptr->px=25;
    for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++)
        cave_feat[10+dy][10+dx]=FEAT_WATER;
    cave_feat[10][10]=FEAT_BRIDGE_WATER_H;
    cave_environment_seed();
    char path[1024];
    snprintf(path,sizeof(path),"%s/bridge-sound.png",out);
    uint64_t sound=render_cell(FEAT_BRIDGE_WATER_H,path);
    cave_environment_flood_bridge(10,10,FEAT_WATER,10);
    snprintf(path,sizeof(path),"%s/bridge-damaged.png",out);
    uint64_t damaged=render_cell(FEAT_BRIDGE_WATER_H,path);
    assert(sound!=damaged);
    cave_environment_flood_bridge(10,10,FEAT_LAVA,1);
    snprintf(path,sizeof(path),"%s/bridge-lava-flood.png",out);
    assert(render_cell(FEAT_BRIDGE_WATER_H,path)!=damaged);
    assert(cave_environment_cell_at(10,10)->material==ENV_BRIDGE_WOOD);
    assert(cave_environment_display_underlay(10,10)==FEAT_LAVA);
    cave_environment_observe(10,10);cave_info[10][10]&=~CAVE_SEEN;
    cave_environment_flood_bridge(10,10,FEAT_POISON,1);
    assert(cave_environment_display_underlay(10,10)==FEAT_LAVA);
    assert(cave_environment_cell_at(10,10)->known_material==ENV_BRIDGE_WOOD);
    cave_environment_reset();
    puts("Living bridge SDL pixels: damaged deck differs, wood remains wood over lava, hidden replacement retains observed underlay PASS");
}

static void bridge_pixels(const char* out) {
    bridge_map();p_ptr->py=p_ptr->px=25;
    uint64_t floor=render_cell(FEAT_FLOOR,NULL),hashes[12];
    for(int i=0;i<6;i++) {
        /* Keep some liquid exposed around the deck. An isolated puddle's
         * small connected shape may be completely hidden by the planks. */
        for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++)
            cave_feat[10+dy][10+dx]=materials[i];
        uint64_t underlay=render_cell(materials[i],NULL);
        for(int axis=0;axis<2;axis++) {
            hashes[i*2+axis]=render_cell(cave_bridge_feature(materials[i],axis),NULL);
            assert(hashes[i*2+axis]!=floor&&hashes[i*2+axis]!=underlay);
        }
        assert(hashes[i*2]!=hashes[i*2+1]);
    }
    for(int i=0;i<12;i++)for(int j=i+1;j<12;j++)assert(hashes[i]!=hashes[j]);
    for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++)
        cave_feat[10+dy][10+dx]=FEAT_WATER;
    p_ptr->blind=true;assert(render_cell(FEAT_BRIDGE_WATER_H,NULL)!=hashes[0]);p_ptr->blind=false;
    p_ptr->rage=true;cave_info[10][10]=CAVE_MARK;
    assert(render_cell(FEAT_BRIDGE_WATER_H,NULL)==render_cell(FEAT_NONE,NULL));p_ptr->rage=false;
    cave_info[10][10]=CAVE_MARK|CAVE_SEEN;
    use_graphics=GRAPHICS_NONE;byte a,ta;char c,tc;
    for(int axis=0;axis<2;axis++) {
        cave_set_feat(10,10,cave_bridge_feature(FEAT_WATER,axis));
        map_info(10,10,&a,&c,&ta,&tc);assert(c==(axis?'|':'='));
    }
    use_graphics=GRAPHICS_MICROCHASM;
    p_ptr->cur_map_hgt=10;p_ptr->cur_map_wid=42;
    for(int y=0;y<10;y++)for(int x=0;x<42;x++) {
        int column=x/7,local=x%7,feat=FEAT_FLOOR;
        if(local>=1&&local<=5&&((y>=1&&y<=3)||(y>=5&&y<=8)))feat=materials[column];
        if(y==2&&local>=1&&local<=5)feat=cave_bridge_feature(materials[column],false);
        if(local==3&&y>=5&&y<=8)feat=cave_bridge_feature(materials[column],true);
        cave_set_feat(y,x,feat);cave_info[y][x]=CAVE_MARK|CAVE_SEEN;cave_light[y][x]=2;
        cave_m_idx[y][x]=cave_o_idx[y][x]=0;
    }
    SDL_Texture* target=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,42*48,10*48);assert(target);SDL_SetRenderTarget(g_state.renderer,target);
    for(int y=0;y<10;y++)for(int x=0;x<42;x++) {
        map_info(y,x,&a,&c,&ta,&tc);SDL_FRect dst={x*48,y*48,48,48};
        sdl_draw_map_tile_layers_at(y,x,a,c,ta,tc,&dst);
    }
    SDL_Surface* surface=SDL_RenderReadPixels(g_state.renderer,NULL);assert(surface);
    char path[1024];snprintf(path,sizeof(path),"%s/bridge-materials.png",out);
    assert(IMG_SavePNG(surface,path));SDL_DestroySurface(surface);
    SDL_SetRenderTarget(g_state.renderer,NULL);SDL_DestroyTexture(target);
    puts("Production SDL pixels:12 distinct oriented decks, underlay identity, dim/hidden views and ASCII identity PASS");
}

int main(int argc,char** argv) {
    assert(argc==2);setbuf(stdout,NULL);log_set_level(LOG_WARN);
    assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS));assert(TTF_Init());
    sdl_config_set_defaults(&config);
    assert(SDL_CreateWindowAndRenderer("Bridge fixture",1024,768,SDL_WINDOW_HIDDEN,&g_state.window,&g_state.renderer));
    g_state.use_tiles=true;assert(sdl_load_tileset_texture());use_graphics=GRAPHICS_MICROCHASM;
    term_init(&test_term,80,30,256);test_term.xtra_hook=dummy_xtra;
    Term=term_screen=&test_term;angband_term[0]=Term;
    #define BRIDGE_GRID(name) name=calloc(MAX_DUNGEON_HGT,sizeof(*name));assert(name)
    BRIDGE_GRID(cave_feat);BRIDGE_GRID(cave_info);BRIDGE_GRID(cave_m_idx);BRIDGE_GRID(cave_o_idx);BRIDGE_GRID(cave_light);
    BRIDGE_GRID(cave_color);BRIDGE_GRID(cave_rewired);BRIDGE_GRID(cave_when);BRIDGE_GRID(cave_natural);
    #undef BRIDGE_GRID
    z_info=calloc(1,sizeof(*z_info));z_info->f_max=102;z_info->o_max=64;z_info->style_max=1;
    f_info=calloc(256,sizeof(*f_info));style_info=calloc(1,sizeof(*style_info));
    style_info[0].name=1;style_info[0].wall_col=4;style_info[0].floor_col=1;
    mon_list=calloc(64,sizeof(*mon_list));r_info=calloc(64,sizeof(*r_info));l_list=calloc(64,sizeof(*l_list));
    inventory=calloc(INVEN_TOTAL,sizeof(*inventory));o_list=calloc(64,sizeof(*o_list));k_info=calloc(64,sizeof(*k_info));
    r_name="\0test creature\0";k_name="\0ration\0";k_info[1].name=1;k_info[1].tval=TV_FOOD;
    k_info[1].x_attr=TILE_FLAG|1;k_info[1].x_char=TILE_FLAG|1;
    for(int i=0;i<256;i++){f_info[i].mimic=i;f_info[i].x_attr=TILE_FLAG;f_info[i].x_char=TILE_FLAG|1;}
    @FEATURE_INIT@
    Rand_state_init(1234);bridge_rules();bridge_save();bridge_pixels(argv[1]);bridge_environment_pixels(argv[1]);
    sdl_idle_animation_shutdown();SDL_Quit();puts("Bridge checks PASS");return 0;
}
'''


def c_function(path,name):
    import re
    source=path.read_text(encoding="utf-8")
    match=re.search(r"(?m)^.*\b"+name+r"\([^;]*?\)\s*\{",source)
    assert match,name
    start=match.start();pos=source.index("{",match.start());depth=1;pos+=1
    while depth:
        depth+=(source[pos]=="{")-(source[pos]=="}");pos+=1
    return source[start:pos]


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    writer=(ROOT/"src/fs/save-dungeon.c").read_text()
    reader=(ROOT/"src/fs/load-dungeon.c").read_text()
    write_rle=writer.split('/*** Simple "Run-Length-Encoding" of cave_feat ***/',1)[1]
    write_rle=write_rle[:write_rle.index('=== END CAVE_FEAT RLE ===')]
    write_rle=write_rle[:write_rle.rfind('log_trace(')]
    read_rle=reader.split('/*** Run length decoding of cave_feat ***/',1)[1].split('/* Back-compat probe:',1)[0]
    guards=c_function(ROOT/"src/fs/load-dungeon.c","read_dungeon_rle_pair")+"\n"+c_function(ROOT/"src/fs/load-dungeon.c","dungeon_rle_pair_status")
    version_policy="static byte fixture_sf_major,fixture_sf_minor,fixture_sf_patch,fixture_sf_extra;\n"
    for name in ("savefile_version_compare","savefile_version_at_least","savefile_version_at_most","savefile_version_supported"):
        function=c_function(ROOT/"src/fs/load.c",name)
        function=function.replace("savefile_version_","bridge_savefile_version_")
        for part in ("major","minor","patch","extra"):
            function=function.replace("sf_"+part,"fixture_sf_"+part)
        version_policy+=function+"\n"
    feature_init=[];feature=None;bridges={}
    for line in (ROOT/"lib/edit/terrain.txt").read_text().splitlines():
        if line.startswith("N:"):
            _,serial,name=line.split(":",2);feature=int(serial)
            if 88 <= feature <= 99:bridges[feature]=name
        elif line.startswith("T:"):
            _,row,col=line.split(":")
            feature_init.append(f'f_info[{feature}].x_attr=TILE_FLAG|{row};f_info[{feature}].x_char=TILE_FLAG|{col};')
        elif line.startswith("G:") and 88<=feature<=99:
            _,symbol,color=line.split(":");feature_init.append(f"f_info[{feature}].d_char='{symbol}';f_info[{feature}].d_attr=TERM_UMBER;")
    assert set(bridges)==set(range(88,100))
    feature_limit = next(int(line.split(":")[2]) for line in
                         (ROOT/"lib/edit/limits.txt").read_text().splitlines()
                         if line.startswith("M:F:"))
    assert feature_limit > max(bridges)
    source=OUT/"check.c"
    source.write_text(HARNESS.replace("@WRITE_RLE@",write_rle).replace("@READ_RLE@",read_rle)
        .replace("@READ_GUARDS@",guards).replace("@VERSION_POLICY@",version_policy)
        .replace("@FEATURE_INIT@","\n".join(feature_init)))
    cmake=BUILD/"CMakeFiles/sil-more.dir"
    objects=shlex.split((cmake/"objects1.rsp").read_text())
    rsp=OUT/"objects.rsp";rsp.write_text("\n".join('"'+p+'"' for p in objects if not p.endswith('/src/main.c.obj')))
    env=os.environ.copy();env['PATH']=';'.join([
        *[str(BUILD/'_deps'/x) for x in ('SDL','SDL_ttf','SDL_image','SDL_mixer')],
        'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    env['SDL_VIDEO_DRIVER']='dummy';env['SDL_RENDER_DRIVER']='software'
    symbols=['handle_stuff','perceive','check_mandos_quest_interaction','check_niena_quest_completion',
        'trigger_chasm_sanctum_ambush_if_needed','update_mon','msg_print','message_flush','object_desc']
    exe=OUT/'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0','-g',
        '@CMakeFiles/sil-more.dir/includes_C.rsp',str(source),'@'+str(rsp),
        '@CMakeFiles/sil-more.dir/linkLibs.rsp',*['-Wl,--wrap='+s for s in symbols],'-o',str(exe)],
        cwd=BUILD,env=env,check=True)
    subprocess.run([str(exe),str(OUT)],cwd=ROOT,env=env,check=True,timeout=60)


if __name__=='__main__':main()

#!/usr/bin/env python3
"""Run production water rules/generation/rendering against an isolated SDL map.

Build first with build-incremental.ps1. Reuses the torch regression harness so
water must also preserve the existing animation, overlays and pan behavior.
No player save/config files are opened.
"""
from pathlib import Path
import os
import shlex
import subprocess
import json
import check_idle_animation as idle

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/water-check"

TESTS = r'''
#include "melee/melee-movement.h"
#include "melee/melee-util.h"

void continue_leap(void);
void generation_tests(void);
void water_test_reset_partition(void);
void monster_save_tests(void);
void vault_water_tests(void);
bool build_vault(int,int,vault_type*,bool);
void __wrap_perceive(void) {}
void __wrap_check_mandos_quest_interaction(void) {}
void __wrap_check_niena_quest_completion(void) {}
void __wrap_trigger_chasm_sanctum_ambush_if_needed(int y,int x) {(void)y;(void)x;}
void __wrap_update_mon(int m, bool full) {(void)m;(void)full;}
void __wrap_msg_print(cptr msg) {(void)msg;}
void __wrap_message_flush(void) {}
void __wrap_place_forge(int y,int x) {cave_set_feat(y,x,FEAT_FORGE_NORMAL_HEAD+3);}
static int artefact_y, artefact_x, artefact_count;
void __wrap_create_chosen_artefact(byte id,int y,int x,bool identify) {
    assert(id==ART_DURIN);(void)identify;
    artefact_y=y;artefact_x=x;artefact_count++;
}

void water_preview(const char* path,int scale) {
    SDL_Texture* previous=SDL_GetRenderTarget(g_state.renderer);
    SDL_Texture* target=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,p_ptr->cur_map_wid*16*scale,p_ptr->cur_map_hgt*16*scale);
    assert(target);SDL_SetRenderTarget(g_state.renderer,target);
    SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255);SDL_RenderClear(g_state.renderer);
    for(int y=0;y<p_ptr->cur_map_hgt;y++)for(int x=0;x<p_ptr->cur_map_wid;x++) {
        byte a,c,ta,tc;
        map_info(y,x,&a,(char*)&c,&ta,(char*)&tc);
        SDL_FRect dst={x*16*scale,y*16*scale,16*scale,16*scale};
        sdl_draw_map_tile_layers_at(y,x,a,c,ta,tc,&dst);
    }
    SDL_Surface* surface=SDL_RenderReadPixels(g_state.renderer,NULL);assert(surface);
    assert(IMG_SavePNG(surface,path));SDL_DestroySurface(surface);
    SDL_SetRenderTarget(g_state.renderer,previous);SDL_DestroyTexture(target);
}

void water_map(int h, int w, int feat) {
    character_dungeon = false;
    p_ptr->cur_map_hgt = h; p_ptr->cur_map_wid = w;
    p_ptr->depth = 5; p_ptr->py = 10; p_ptr->px = 10;
    p_ptr->leaping = p_ptr->blind = p_ptr->rage = false;
    p_ptr->truce = false; p_ptr->total_weight = 0;
    p_ptr->chp = p_ptr->mhp = 100;
    p_ptr->wy = p_ptr->wx = 0; p_ptr->is_dead = false;
    g_labyrinth_view_active = false;
    for (int y=0; y<h; y++) for(int x=0; x<w; x++) {
        cave_set_feat(y,x,feat);
        cave_color[y][x] = COLOR_STYLE_BASE;
        cave_info[y][x] |= CAVE_MARK | CAVE_SEEN;
        cave_natural[y][x] = cave_when[y][x] = 0;
        cave_m_idx[y][x] = cave_o_idx[y][x] = 0;
        cave_light[y][x] = 2;
    }
    water_test_reset_partition();
    cave_fixtures_clear(); sdl_idle_animation_clear_cells();
}

static void scent_tests(void) {
    water_map(32,32,FEAT_FLOOR);
    scent_when = 100;
    for(int y=1;y<31;y++) cave_set_feat(y,11,FEAT_WATER);
    update_smell();
    assert(cave_when[10][10] != 0 && cave_when[10][11] == 0);
    assert(cave_when[10][12] == 0 && cave_when[9][12] == 0);
    byte old_bank = cave_when[10][10];
    p_ptr->px = 11; cave_when[10][11] = 70; update_smell();
    assert(cave_when[10][11] == 0 && cave_when[10][10] == old_bank);
    assert(cave_when[10][12] == 0);
    p_ptr->px = 12; update_smell();
    assert(cave_when[10][12] && cave_when[10][10] == old_bank);
    cave_when[10][11] = scent_when; assert(get_scent(10,11) == -1);
    cave_set_feat(10,12,FEAT_WATER); assert(cave_when[10][12] == 0);
    p_ptr->py = p_ptr->px = 20; p_ptr->leaping = true; update_smell();
    assert(cave_when[20][20] == 0); p_ptr->leaping = false;
    puts("Scent: both banks, wet entry, old land tracks, tracker and airborne midpoint: PASS");
}

static void movement_tests(void) {
    water_map(32,32,FEAT_FLOOR);
    cave_m_idx[10][10] = -1;
    cave_set_feat(10,11,FEAT_WATER); cave_set_feat(10,12,FEAT_WATER);
    p_ptr->active_ability[S_EVN][EVN_LEAPING] = false;
    for(int step=0;step<4;step++) {
        p_ptr->energy_use=100; stealth_score=20; move_player(6);
        if(p_ptr->px != 11+step) fprintf(stderr,"step=%d pos=(%d,%d) energy=%d feat=%d info=%x\n",step,p_ptr->py,p_ptr->px,p_ptr->energy_use,cave_feat[10][11],cave_info[10][11]);
        assert(p_ptr->px == 11+step);
        assert(p_ptr->energy_use == (step<3 ? 150 : 100));
        assert(stealth_score == (step<3 ? 17 : 20));
    }
    p_ptr->energy_use=200; stealth_score=20;
    player_water_movement(FEAT_WATER,FEAT_WATER);
    assert(p_ptr->energy_use==300 && stealth_score==17);
    assert(water_movement_energy(100,FEAT_WATER,FEAT_WATER,true)==100);
    /* A real leap has two turns; neither touches its water midpoint. */
    water_map(32,32,FEAT_FLOOR); cave_m_idx[10][10]=-1;
    cave_set_feat(10,11,FEAT_WATER);
    p_ptr->active_ability[S_EVN][EVN_LEAPING]=true;
    p_ptr->previous_action[1]=6; p_ptr->energy_use=100; stealth_score=20;
    move_player(6);
    assert(p_ptr->px==11 && p_ptr->leaping && p_ptr->energy_use==100 && stealth_score==20);
    continue_leap();
    assert(p_ptr->px==12 && !p_ptr->leaping && p_ptr->energy_use==100 && stealth_score==15);
    /* Landing blocked after takeoff: exactly one wet landing charge. */
    cave_m_idx[10][12]=0; cave_m_idx[10][10]=-1; p_ptr->px=10;
    p_ptr->energy_use=100; stealth_score=20; move_player(6);
    cave_set_feat(10,12,FEAT_WALL_EXTRA); continue_leap();
    assert(p_ptr->px==11 && !p_ptr->leaping && p_ptr->energy_use==150 && stealth_score==12);
    p_ptr->active_ability[S_EVN][EVN_LEAPING]=false;
    /* Actual monster movement leaves signed energy debt, without slowing flight. */
    water_map(32,32,FEAT_FLOOR); p_ptr->py=p_ptr->px=25;
    monster_type* m=&mon_list[1]; memset(m,0,sizeof(*m));
    m->r_idx=1; m->fy=10; m->fx=10; m->hp=10; m->maxhp=10;
    m->alertness=ALERTNESS_ALERT; cave_m_idx[10][10]=1;
    cave_set_feat(10,11,FEAT_WATER); cave_set_feat(10,12,FEAT_WATER);
    r_info[1].flags2=0; process_move(m,10,11,false);
    assert(m->fx==11 && m->energy==-50 && m->noise>=8);
    m->energy=0; process_move(m,10,11,false); assert(m->energy==0);
    m->energy=0; process_move(m,10,12,false); assert(m->energy==-50);
    m->energy=0; process_move(m,10,13,false); assert(m->energy==-50);
    m->energy=0; r_info[1].flags2=RF2_FLYING;
    process_move(m,10,12,false); assert(m->energy==0);
    r_info[1].flags2=0;
    puts("Movement: enter/cross/exit, actual two-turn/blocked leaps, ground/flying monsters: PASS");
}

void generation_tests(void) {
    static dun_data dungeon;
    dun=&dungeon;
    int wet_seeds=0, river_seeds=0;
    for(int seed=1;seed<=100;seed++) {
        water_map(64,96,FEAT_WALL_EXTRA); memset(dun,0,sizeof(*dun));
        memset(room_anchor_kind,0,sizeof(room_anchor_kind));
        Rand_state_init(seed); layout_anchor_count=0;
        for(int i=0;i<6;i++) {
            int y=2+(i/3)*29,x=2+(i%3)*30;
            for(int t=0;t<20;t++)
                if(carve_ca_blob_anchor_bounds(y,y+26,x,x+27,0)) break;
        }
        /* Authored and special cells must survive inside otherwise valid cave bounds. */
        cave_set_feat(30,45,FEAT_MORE); cave_set_feat(31,45,FEAT_FORGE_NORMAL_HEAD);
        cave_set_feat(29,45,FEAT_FLOOR); cave_info[29][45]|=CAVE_G_VAULT;
        place_cave_water();
        int wet=0, river=0;
        for(int y=1;y<63;y++)for(int x=1;x<95;x++) if(cave_feat[y][x]==FEAT_WATER) {
            wet++; assert(normal_cave_area(y,x)); assert(cave_natural[y][x]);
            assert(cave_floor_bold(y,x));
            bool has_neighbor=false, in_room=false;
            for(int d=0;d<4;d++) has_neighbor |= cave_feat[y+water_dy[d]][x+water_dx[d]]==FEAT_WATER;
            assert(has_neighbor);
            for(int i=0;i<dun->cent_n;i++) {
                rectangle b=dun->corner[i];
                if(y>=b.y1&&y<=b.y2&&x>=b.x1&&x<=b.x2)in_room=true;
            }
            river += !in_room;
        }
        wet_seeds += wet>0; river_seeds += river>0;
        assert(cave_feat[30][45]==FEAT_MORE && cave_feat[31][45]==FEAT_FORGE_NORMAL_HEAD);
        assert(cave_feat[29][45]==FEAT_FLOOR);
        if(river>0 && river_seeds==1) {
            water_preview("scripts/output/water-check/cave-water.png",1);
            FILE* out=fopen("scripts/output/water-check/generated-map.txt","w");assert(out);
            for(int y=0;y<64;y++) {for(int x=0;x<96;x++) fputc(cave_feat[y][x]==FEAT_WATER?'~':
                cave_floor_bold(y,x)?'.':'#',out);fputc('\n',out);}fclose(out);
        }
        /* Run the same placement pass in every excluded partition type. */
        for(int mode=QUAD_MODE_ROOMY;mode<=QUAD_MODE_BIG_CAVE;mode++) {
            if(mode==QUAD_MODE_CAVEY)continue;
            for(int y=0;y<64;y++)for(int x=0;x<96;x++)
                if(cave_feat[y][x]==FEAT_WATER)cave_set_feat(y,x,FEAT_FLOOR);
            current_partition_modes[0]=(quadrant_mode_t)mode;
            place_cave_water();
            for(int y=0;y<64;y++)for(int x=0;x<96;x++)assert(cave_feat[y][x]!=FEAT_WATER);
        }
    }

    /* Real big-cave geometry exceeds the old 24x24 pool bound. Both shapes
     * stay in the ice partition and never replace authored/protected cells. */
    int icy_seeds=0, icy_min=9999, icy_max=0, icy_total=0, cave_floor_total=0;
    for(int seed=1;seed<=100;seed++) {
        water_map(64,96,FEAT_WALL_EXTRA); memset(dun,0,sizeof(*dun));
        memset(room_anchor_kind,0,sizeof(room_anchor_kind));
        Rand_state_init(seed); layout_anchor_count=0;
        current_partition_modes[0]=QUAD_MODE_BIG_CAVE;
        current_partition_big_cave_types[0]=BIG_CAVE_ICE;
        assert(carve_big_cave_bounds(2,61,2,93,0,BIG_CAVE_ICE));
        byte before[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
        for(int y=0;y<64;y++)for(int x=0;x<96;x++)before[y][x]=cave_feat[y][x];
        cave_set_feat(30,45,FEAT_MORE); cave_set_feat(31,45,FEAT_FORGE_NORMAL_HEAD);
        cave_set_feat(29,45,FEAT_FLOOR); cave_info[29][45]|=CAVE_G_VAULT;
        cave_set_feat(28,45,FEAT_FLOOR); cave_o_idx[28][45]=1;
        cave_set_feat(27,45,FEAT_FLOOR); cave_m_idx[27][45]=1;
        place_cave_water();
        int icy=0;
        for(int y=1;y<63;y++)for(int x=1;x<95;x++) {
            cave_floor_total+=before[y][x]==FEAT_FLOOR;
            assert(cave_feat[y][x]!=FEAT_WATER);
            if(cave_feat[y][x]!=FEAT_ICE)continue;
            icy++; assert(before[y][x]==FEAT_FLOOR && water_area(y,x,0));
            assert(cave_floor_bold(y,x));
            bool neighbor=false;
            for(int d=0;d<4;d++)neighbor |= cave_feat[y+water_dy[d]][x+water_dx[d]]==FEAT_ICE;
            assert(neighbor);
            assert(distance(y,x,dun->cent[0].y,dun->cent[0].x)>1);
        }
        icy_seeds+=icy>0;
        icy_min=MIN(icy_min,icy);icy_max=MAX(icy_max,icy);icy_total+=icy;
        if(seed==1) {
            /* Preview production cavern geometry and ice placement at native
             * tile size; the isolated harness supplies its usual base style. */
            cave_o_idx[28][45]=cave_m_idx[27][45]=0;
            water_preview("scripts/output/water-check/cave-ice.png",1);
        }
        assert(icy>=24);
        assert(cave_feat[30][45]==FEAT_MORE && cave_feat[31][45]==FEAT_FORGE_NORMAL_HEAD);
        assert(cave_feat[29][45]==FEAT_FLOOR && cave_feat[28][45]==FEAT_FLOOR && cave_feat[27][45]==FEAT_FLOOR);
    }
    /* A touching cave in the next partition cannot be reached by ice paths. */
    water_map(40,80,FEAT_FLOOR); memset(dun,0,sizeof(*dun));
    current_partition_rows=1;current_partition_cols=current_partition_count=2;
    current_partition_modes[0]=QUAD_MODE_BIG_CAVE;
    current_partition_big_cave_types[0]=BIG_CAVE_ICE;
    current_partition_modes[1]=QUAD_MODE_CAVEY;
    current_partition_big_cave_types[1]=BIG_CAVE_NONE;
    dun->cent_n=2;dun->cent[0]=(coord){20,20};
    dun->cent[1]=(coord){15,15};room_anchor_kind[1]=LAYOUT_ANCHOR_NONE;
    dun->corner[0]=(rectangle){2,2,37,77};
    room_anchor_kind[0]=LAYOUT_ANCHOR_CA_BLOB;
    for(int y=1;y<39;y++)for(int x=1;x<79;x++)cave_info[y][x]|=CAVE_ROOM;
    assert(water_area(20,20,0) && !water_area(20,60,0));
    assert(!lake_floor(15,15,0,0) && !lake_floor(15,16,0,0));
    coord crossing[WATER_PATH_MAX];
    assert(river_path((coord){10,10},(coord){10,60},crossing,false,0)==0);
    place_cave_water();
    for(int y=1;y<39;y++)for(int x=1;x<79;x++)
        if(cave_feat[y][x]==FEAT_ICE) {
            assert(level_partition_index_for_point(y,x)==0);
            assert(distance(y,x,15,15)>1);
        }
    printf("Ice generation: %d/100 real large caverns; connected shapes, anchors, protected features and partition boundary: PASS\n",icy_seeds);

    printf("Ice density: %d-%d cells/cavern, %.1f mean, %.1f%% of generated cavern floor\n",
        icy_min,icy_max,icy_total/100.0,100.0*icy_total/cave_floor_total);
    assert(wet_seeds>75 && river_seeds>20);
    printf("Generation: 100 real CA cave layouts, %d wet, %d with connecting rivers; excluded partitions/features: PASS\n",wet_seeds,river_seeds);
}

static void water_render_tests(void) {
    water_map(32,32,FEAT_FLOOR); character_dungeon=character_generated=true;
    p_ptr->py=p_ptr->px=15; use_bigtile=false;
    Term->total_erase=true;prt_map();Term_fresh();
    cave_set_feat(10,11,FEAT_WATER); cave_set_feat(10,12,FEAT_WATER);
    f_info[FEAT_WATER].x_attr=TILE_FLAG; f_info[FEAT_WATER].x_char=(char)(TILE_FLAG|1);
    Term->soft_cursor=false;
    frame_tick=0; draw_cell(10,11,false); draw_cell(10,12,false);
    assert(water_texture && cell_count==2);
    u64b rng=Rand_state_export(); s16b energy=p_ptr->energy;
    int loads=image_loads, textures=texture_creations;
    SDL_Surface* frames[4];
    for(int i=0;i<4;i++) {
        frame_tick=(Uint64)i*8; draw_cell(10,11,false);draw_cell(10,12,false);
        frames[i]=capture(40+i);
        if(i) assert(!same_surface(frames[i-1],frames[i]));
    }
    assert(sdl_idle_animation_timeout_ms(32*IDLE_STEP_NS)==0);
    sdl_idle_animation_update(32*IDLE_STEP_NS);
    assert(p_ptr->energy==energy && Rand_state_export()==rng);
    assert(image_loads==loads && texture_creations==textures);
    for(int big=0;big<2;big++) {
        use_bigtile=big;
        /* Match the full erase issued by the actual tile-width setting. */
        Term->total_erase=true;
        for(int pan=0;pan<3;pan++) {
            p_ptr->wx=pan;prt_map();Term_fresh();
            SDL_Surface* incremental=capture(50);
            force_map_redraw();Term_fresh();
            SDL_Surface* full=capture(51);
            assert(same_surface(incremental,full));
            SDL_DestroySurface(incremental);SDL_DestroySurface(full);
        }
    }
    use_bigtile=false;Term->total_erase=true;p_ptr->wx=0;prt_map();Term_fresh();
    /* Erasing a water tile must also erase its animation pixels. */
    cave_set_feat(10,11,FEAT_FLOOR);Term_fresh();
    SDL_Surface* erased=capture(52);force_map_redraw();Term_fresh();
    SDL_Surface* repaint=capture(53);assert(same_surface(erased,repaint));
    SDL_DestroySurface(erased);SDL_DestroySurface(repaint);
    cave_info[10][11]&=~CAVE_SEEN; cave_info[10][12]&=~CAVE_SEEN;
    assert(sdl_idle_animation_timeout_ms(40*IDLE_STEP_NS)==-1);
    cave_info[10][11]=0;assert(!visible_liquid(10,11));
    for(int i=0;i<4;i++)SDL_DestroySurface(frames[i]);
    puts("Water pixels: four frames, idle redraw, pan/erase in both tile widths, fog of war, no turn/RNG/I/O activity: PASS");
}

static void water_tests(void) {
    cave_when=calloc(MAX_DUNGEON_HGT,sizeof(*cave_when));
    cave_natural=calloc(MAX_DUNGEON_HGT,sizeof(*cave_natural));
    mon_list=calloc(64,sizeof(*mon_list)); r_info=calloc(64,sizeof(*r_info));
    l_list=calloc(64,sizeof(*l_list)); inventory=calloc(INVEN_TOTAL,sizeof(*inventory));
    o_list=calloc(64,sizeof(*o_list)); k_info=calloc(64,sizeof(*k_info));
    r_name="\0test creature\0"; r_info[1].name=1; mon_max=1;
    scent_tests(); movement_tests(); generation_tests(); monster_save_tests();
    vault_water_tests(); water_render_tests();
}
'''

VAULT_TEST = r'''
void vault_water_tests(void) {
    vault_type vault={0};
    static char rows[]=@ROWS@;
    v_text=rows;v_name="Kheled-Zaram";
    vault.typ=8;vault.hgt=@HEIGHT@;vault.wid=@WIDTH@;
    vault.flags=VLT_LIGHT|VLT_SURFACE;
    vault.style_count=1;vault.style_idx[0]=0;vault.style_weight[0]=1;
    /* Use the source vault's style with the harness's single style slot. */
    style_info[0].wall_row=@WALL_ROW@;style_info[0].wall_col=@WALL_COL@;
    style_info[0].floor_row=@FLOOR_ROW@;style_info[0].floor_col=@FLOOR_COL@;
    style_info[0].floor_count=0;
    for(int rotation=0;rotation<2;rotation++)for(int seed=0;seed<8;seed++) {
        water_map(23,23,FEAT_WALL_EXTRA);Rand_state_init(seed);artefact_count=0;
        assert(build_vault(11,11,&vault,rotation));
        int wet=0;
        for(int y=0;y<23;y++)for(int x=0;x<23;x++) {
            assert(cave_feat[y][x]!=FEAT_CHASM);
            if(cave_feat[y][x]==FEAT_WATER) {wet++;assert(cave_info[y][x]&CAVE_ICKY);}
        }
        assert(wet==@WET@&&artefact_count==1);
        assert(cave_feat[artefact_y][artefact_x]==FEAT_FLOOR);
        if(!rotation&&seed==0)water_preview("scripts/output/water-check/kheled-zaram.png",2);
    }
    puts("Kheled-Zaram: actual vault builder, 16 rotations/reflections, lake tiles and dry Durin artefact square: PASS");
    style_info[0].wall_row=0;style_info[0].wall_col=4;
    style_info[0].floor_row=0;style_info[0].floor_col=1;
}
'''

SAVE_TEST = r'''
#include "angband.h"
#include <assert.h>
#include <stdio.h>
static byte bytes[2048];
static int write_pos, read_pos, extra_version;
static const bool savefile_has_song_duels=true, savefile_has_monster_shatter=true;
static const bool savefile_has_thrall_quest=true, savefile_has_thrall_quest_requested=true;
static void wr_byte(byte x) { assert(write_pos < 2048); bytes[write_pos++]=x; }
static void wr_u16b(u16b x) {wr_byte(x);wr_byte(x>>8);}
static void wr_s16b(s16b x) {wr_u16b((u16b)x);}
static void wr_u32b(u32b x) {wr_u16b(x);wr_u16b(x>>16);}
static void wr_s32b(s32b x) {wr_u32b((u32b)x);}
static void rd_byte(byte* x) {assert(read_pos<write_pos);*x=bytes[read_pos++];}
static void rd_u16b(u16b* x) {byte a,b;rd_byte(&a);rd_byte(&b);*x=a|((u16b)b<<8);}
static void rd_s16b(s16b* x) {u16b n;rd_u16b(&n);*x=(s16b)n;}
static void rd_u32b(u32b* x) {u16b a,b;rd_u16b(&a);rd_u16b(&b);*x=a|((u32b)b<<16);}
static void rd_s32b(s32b* x) {u32b n;rd_u32b(&n);*x=(s32b)n;}
static void strip_bytes(int n) {while(n--) {byte b;rd_byte(&b);}}
static bool savefile_version_at_least(byte a,byte b,byte c,byte d) {
    assert(a==0&&b==9&&c==8&&(d==2||d==4));return extra_version>=d;
}
/* The complete production record writer and reader are inserted below. */
@FUNCTIONS@
void monster_save_tests(void) {
    for(int version=1;version<=4;version++) {
        monster_type before={0}, after={0};
        before.r_idx=3;before.image_r_idx=7;before.fy=14;before.fx=19;
        before.hp=39;before.maxhp=46;before.alertness=ALERTNESS_ALERT;
        before.energy=version>=2?-50:213;before.mspeed=2;before.stunned=9;
        before.poisoned=17;
        before.confused=6;before.song_will_penalty=11;before.thrall_quest_completed=1;
        before.previous_action[0]=6;before.previous_action[1]=8;
        extra_version=version;write_pos=read_pos=0;wr_monster(&before);
        /* Poison was appended to the record in 0.9.8.4. */
        if(version<4)write_pos-=2;
        if(version==1) {
            /* Old byte-energy record: omit its high byte, preserve its tail. */
            assert(bytes[14]==213 && bytes[15]==0);
            memmove(bytes+15,bytes+16,write_pos-16);write_pos--;
        }
        rd_monster(&after);
        assert(read_pos==write_pos);
        assert(after.energy==before.energy && after.hp==before.hp);
        assert(after.r_idx==3&&after.fx==19&&after.stunned==9&&after.confused==6);
        assert(after.song_will_penalty==11 && after.thrall_quest_completed==1);
        assert(after.previous_action[1]==8);
        assert(after.poisoned==(version>=4?17:0));
    }
    puts("Monster save records: versions 0.9.8.1-4, signed debt, old unsigned energy, poison defaults and complete record alignment: PASS");
}
'''

def c_function(path, name):
    source = path.read_text(encoding="utf-8")
    start = source.index("void " + name + "(")
    pos = source.index("{", start)
    depth = 1
    end = pos + 1
    while depth:
        if source[end] == "{":
            depth += 1
        elif source[end] == "}":
            depth -= 1
        end += 1
    return source[start:end]

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    idle.OUT.mkdir(parents=True, exist_ok=True)
    # Keep the original fixture regression suite and its minimal environment.
    gen_start = TESTS.index("void generation_tests(void) {")
    gen_end = TESTS.index("static void water_render_tests(void)")
    gen = OUT / "generation-check.c"
    gen.write_text('#include "level-generation/level-generation-water.c"\n#include <assert.h>\n'
        'void water_map(int,int,int);\n'
        'void water_preview(const char*,int);\n'
        'void water_test_reset_partition(void) { current_partition_rows = current_partition_cols = current_partition_count = 1; current_partition_modes[0] = QUAD_MODE_CAVEY; current_partition_big_cave_types[0] = BIG_CAVE_NONE; }\n'
        + TESTS[gen_start:gen_end], encoding="utf-8")
    vault_text = (ROOT / "lib/edit/vault.txt").read_text(encoding="utf-8").split("N:407:Kheled-Zaram", 1)[1].split("\nN:", 1)[0]
    rows = [line[2:] for line in vault_text.splitlines() if line.startswith("D:")]
    assert len({len(row) for row in rows}) == 1
    style = (ROOT / "lib/edit/style.txt").read_text(encoding="utf-8").split("N:57:", 1)[1].split("\nN:", 1)[0]
    wall = next(line[2:].split(":") for line in style.splitlines() if line.startswith("W:"))
    floor = next(line[2:].split()[0].split(":") for line in style.splitlines() if line.startswith("F:"))
    vault_test = VAULT_TEST
    values = {"ROWS": json.dumps("".join(rows)), "HEIGHT": len(rows), "WIDTH": len(rows[0]),
              "WET": sum(row.count("_") for row in rows), "WALL_ROW": wall[0], "WALL_COL": wall[1],
              "FLOOR_ROW": floor[0], "FLOOR_COL": floor[1]}
    for key, value in values.items():
        vault_test = vault_test.replace("@" + key + "@", str(value))
    terrain_init = []
    feature = None
    for line in (ROOT / "lib/edit/terrain.txt").read_text(encoding="utf-8").splitlines():
        if line.startswith("N:"):
            feature = int(line.split(":")[1])
        elif line.startswith("T:"):
            row, col = map(int, line[2:].split(":"))
            terrain_init.append(f'f_info[{feature}].x_attr=TILE_FLAG|{row};f_info[{feature}].x_char=(char)(TILE_FLAG|{col});')
    harness = idle.HARNESS.replace("int main(void) {", TESTS[:gen_start] + TESTS[gen_end:] + vault_test + "\nint main(void) {")
    harness = harness.replace("    sdl_idle_animation_shutdown();\n    SDL_Quit();", "    water_tests();\n    sdl_idle_animation_shutdown();\n    SDL_Quit();")
    harness = harness.replace("    water_tests();", "    " + "".join(terrain_init) + "\n    water_tests();")
    source = OUT / "check.c"
    source.write_text(harness, encoding="utf-8")
    save_check = OUT / "monster-save-check.c"
    writer_source = (ROOT / "src/fs/save.c").read_text(encoding="utf-8")
    saved_flags = writer_source[writer_source.index("#define SAVE_MON_FLAGS"):]
    saved_flags = saved_flags[:saved_flags.index("\n\n")]
    save_check.write_text(SAVE_TEST.replace("@FUNCTIONS@", saved_flags + "\n" +
        c_function(ROOT / "src/fs/save.c", "wr_monster") + "\n" +
        c_function(ROOT / "src/fs/load.c", "rd_monster")), encoding="utf-8")
    wrappers = [str(gen), str(save_check)]
    for operation, internal in (("write", "wr"), ("read", "rd")):
        stem = "save" if operation == "write" else "load"
        p = OUT / (stem + "-check.c")
        result, ret = ("void", "") if operation == "write" else ("errr", "return ")
        p.write_text(f'#include "fs/{stem}-dungeon.c"\n{result} test_{operation}_fixtures(void) {{ {ret}{internal}_fixtures(); }}\n', encoding="utf-8")
        wrappers.append(str(p))
    menu = OUT / "settings-check.c"
    menu.write_text('#include "cmd/ui/cmd-ui-settings.c"\n'
        'bool test_pick_torch_option(bool* handled) { return option_pick_value(OPT_torch_animation_always, handled); }\n'
        'void test_reset_torch_option(bool* app_dirty) { const int opt[] = { OPT_torch_animation_always }; bool meta = false, sound = false; options_aux_reset_to_default(VISUAL_PAGE, opt, 0, false, NULL, app_dirty, &sound, &meta); }\n', encoding="utf-8")
    wrappers.append(str(menu))
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    exclude = ("/src/main.c.obj", "/src/sdl/render/sdl-idle-animation.c.obj", "/src/fs/save-dungeon.c.obj",
               "/src/fs/load-dungeon.c.obj", "/src/cmd/ui/cmd-ui-settings.c.obj", "/src/level-generation/level-generation-water.c.obj")
    rsp = OUT / "objects.rsp"
    rsp.write_text("\n".join('"' + p + '"' for p in objects if not p.endswith(exclude)), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(["C:/msys64/mingw64/bin", "C:/msys64/usr/bin",
        *[str(BUILD / "_deps" / x) for x in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")], env["PATH"]])
    env["SDL_VIDEO_DRIVER"] = "dummy"
    env["SDL_RENDER_DRIVER"] = "software"
    symbols = ("save_wr_byte", "save_wr_u16b", "load_rd_byte", "load_rd_u16b", "load_savefile_version_at_least",
        "load_note", "sdl_present_if_needed", "ui_question_ask_overlay_buttons", "perceive",
        "check_mandos_quest_interaction", "check_niena_quest_completion", "trigger_chasm_sanctum_ambush_if_needed",
        "update_mon", "msg_print", "message_flush", "place_forge", "create_chosen_artefact")
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), *wrappers,
        "@" + str(rsp), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
        *["-Wl,--wrap=" + s for s in symbols], "-o", str(exe)], cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe)], cwd=ROOT, env=env, check=True, timeout=60)

if __name__ == "__main__":
    main()

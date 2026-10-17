#!/usr/bin/env python3
"""Full-engine cache/vertical-state persistence and neighbor inheritance checks.

Requires a current Windows build. The fixture compiles the real cache source
against engine objects and real save/load primitives; no cache, geometry,
population or serializer is mocked. Maps explicitly assembled by tests are
identified separately from seeded generate_cave runs. Player files are unused.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/vertical-cache"

WRITER = r'''
#include "fs/save.c"
#include <assert.h>
void fixture_write_begin(byte *buffer,size_t capacity) {
    fff=SDL_IOFromMem(buffer,capacity);assert(fff);
    xor_byte=0;v_stamp=x_stamp=save_byte_offset=0;write_error=false;
}
size_t fixture_write_end(void) {
    size_t length=(size_t)SDL_TellIO(fff);assert(!write_error);
    SDL_CloseIO(fff);fff=NULL;return length;
}
'''

LEGACY_WRITER = r'''
void fixture_legacy_monster(const monster_type*);
void __real_save_wr_monster(const monster_type*);
static bool fixture_legacy_monsters;
void fixture_set_legacy_monsters(bool legacy) { fixture_legacy_monsters=legacy; }
void __wrap_save_wr_monster(const monster_type* monster) {
    if(fixture_legacy_monsters)fixture_legacy_monster(monster);
    else __real_save_wr_monster(monster);
}
'''

READER = r'''
#include "fs/load.c"
#include <assert.h>
void fixture_read_begin(const byte *buffer,size_t length,int extra) {
    fff=SDL_IOFromConstMem(buffer,length);assert(fff);
    xor_byte=0;v_check=x_check=load_byte_offset=0;
    sf_major=VERSION_MAJOR;sf_minor=VERSION_MINOR;sf_patch=VERSION_PATCH;sf_extra=extra;
    savefile_has_runtime_overrides=savefile_has_monster_shatter=true;
    savefile_has_song_duels=savefile_has_thrall_quest=true;
    savefile_has_thrall_quest_requested=savefile_has_cave_info_hi=true;
    savefile_has_cave_rewired=savefile_has_cave_natural=true;
    savefile_has_cave_water_flow=savefile_has_cave_flood_trap_kinds=true;
    savefile_has_item_bonuses=true;
    savefile_has_partition_meta=savefile_has_partition_meta_types=true;
    savefile_has_skeleton_notes=savefile_has_skeleton_hint_mask=true;
    savefile_has_skeleton_hint_mask32=savefile_has_skeleton_hint_counts=true;
    savefile_has_hint_messages=savefile_has_hint_message_meta=true;
    savefile_has_hint_message_destinations=true;
    objects_count_prefetch=0xFFFF;color_rle_pair_prefetched=false;
}
size_t fixture_read_end(void) {
    size_t length=(size_t)SDL_TellIO(fff);
    SDL_CloseIO(fff);fff=NULL;return length;
}
'''

TESTS = r'''
#include "achievements.h"
#include "cave/cave-fixtures.h"
#include "cave/cave-flood.h"
#include "cave/cave-water-flow.h"
#include "level-generation/level-generation-terrain-history.h"
#include "level-generation/level-generation-terrain-vaults.h"
#include "level-generation/level-cache.c"

void fixture_write_begin(byte*,size_t);
void fixture_set_legacy_monsters(bool);
size_t fixture_write_end(void);
void fixture_read_begin(const byte*,size_t,int);
size_t fixture_read_end(void);
static byte encoded[8*1024*1024], plain[8*1024*1024], altered[8*1024*1024];
static level_job next_job;
static unsigned failure_counts[5];
static bool baseline_probe;
static void record_failure(int index)
{
    failure_counts[index]++;
    if(failure_counts[index]>1)return;
    printf("First real failure: stage=%d depth=%d map=%ux%u player=%u,%u\n",index,p_ptr->depth,
        p_ptr->cur_map_hgt,p_ptr->cur_map_wid,p_ptr->py,p_ptr->px);
    if(index==0) {
        int (*seen)[MAX_DUNGEON_WID]=SDL_calloc(MAX_DUNGEON_HGT,sizeof(*seen));assert(seen);
        flood_access(p_ptr->py,p_ptr->px,seen,false);int reported=0;
        for(int y=1;y<p_ptr->cur_map_hgt-1&&reported<5;y++)
            for(int x=1;x<p_ptr->cur_map_wid-1&&reported<5;x++)
                if(!seen[y][x]&&cave_floor_bold(y,x)&&player_passable(y,x,false)) {
                    printf("First failed link unreachable: %d,%d feat=%u info=%u N/S/W/E=%u/%u/%u/%u link=%u\n",
                        y,x,cave_feat[y][x],cave_info[y][x],cave_feat[y-1][x],cave_feat[y+1][x],cave_feat[y][x-1],cave_feat[y][x+1],next_job.links.feat[y][x]);reported++;
                }
        SDL_free(seen);
    }
}
/* Link wrappers only record the unchanged real return values. */
#define TRACK(name,index) bool __real_##name(void); \
bool __wrap_##name(void) { bool result=__real_##name(); \
    if(!result)record_failure(index); return result; }
TRACK(level_cache_generation_links,0)
TRACK(cave_vertical_generation_finish,1)
TRACK(check_connectivity,2)
TRACK(place_rubble_player,3)
TRACK(terrain_history_finish,4)
#undef TRACK

/* Observe production upkeep without dereferencing the deliberately invalid
 * pre-player-placement position on the historical crashing baseline. All
 * ready-scene view rebuilds and calls outside this probe remain real. */
static bool upkeep_probe;
static int upkeep_view_calls,upkeep_lore_calls,upkeep_quest_calls;
void __real_update_view(void);
void __wrap_update_view(void) {
    if(upkeep_probe) {
        upkeep_view_calls++;
        if(!p_ptr->py||!p_ptr->px)return;
    }
    __real_update_view();
}
void __real_update_lore(u32b flags);
void __wrap_update_lore(u32b flags) {
    if(upkeep_probe){upkeep_lore_calls++;return;}
    __real_update_lore(flags);
}
void __real_metarun_check_and_update_quests(void);
void __wrap_metarun_check_and_update_quests(void) {
    if(upkeep_probe){upkeep_quest_calls++;return;}
    __real_metarun_check_and_update_quests();
}

static void fresh(int depth)
{
    character_dungeon=false;
    memset(p_ptr,0,sizeof(*p_ptr));
    reset_map(depth);
    p_ptr->cur_map_hgt=p_ptr->cur_map_wid=88;
    p_ptr->py=p_ptr->px=5;p_ptr->chp=p_ptr->mhp=100;
    p_ptr->max_depth=depth;p_ptr->playing=true;
    p_ptr->food=PY_FOOD_FULL;
    turn=playerturn=1234;
    for(int y=0;y<88;y++)for(int x=0;x<88;x++) {
        bool border=!y||!x||y==87||x==87;
        cave_feat[y][x]=border?FEAT_WALL_PERM:FEAT_FLOOR;
        cave_info[y][x]=border?CAVE_WALL:0;
        cave_color[y][x]=cave_when[y][x]=cave_natural[y][x]=cave_rewired[y][x]=0;
    }
    partition_meta_save parts={0};
    parts.grid_rows=parts.grid_cols=parts.partition_count=1;
    level_partition_meta_set(&parts);
    cave_environment_reset();cave_atmosphere_reset();
    cave_fixtures_clear();cave_flood_clear();cave_water_flow_reset();
    cave_vertical_reset();vertical_vault_reset();
    terrain_generation_reset();terrain_history_reset();terrain_vault_reset();
    styles_init_for_level();
    scent_when=100;
    for(int i=FLOW_WANDERING_HEAD;i<=FLOW_WANDERING_TAIL;i++) {
        flow_center_y[i]=flow_center_x[i]=0;wandering_pause[i]=0;
    }
}

static level_image *capture(void)
{
    level_image *image=level_image_new();assert(image);
    level_image_capture(image);image->visited=true;return image;
}

static vertical_vault_state blueprint(void)
{
    vertical_vault_state state={0};state.count=1;
    state.placements[0]=(vertical_vault_placement){
        .identity=0x81525001U,.upper_template=525,.lower_template=526,
        .upper_depth=8,.y=8,.x=8,.transform=5 };
    return state;
}

static void build_upper_state(bool claimed)
{
    fresh(8);
    vertical_vault_state pair=blueprint();
    vertical_vault_begin(8,false);vertical_vault_inherit(&pair,8);
    cave_vertical_generation_begin(false);
    assert(vertical_vault_place());
    assert(cave_vertical_generation_finish());
    assert(vertical_vault_get()->placements[0].reward_layers==0);
    vertical_vault_marker reward;bool found=false;
    for(int i=0;vertical_vault_marker_at(i,&reward);i++)
        if(reward.role==VERTICAL_VAULT_DRAIN_REWARD){found=true;break;}
    assert(found&&cave_feat[reward.y][reward.x]==FEAT_DEEP_WATER&&!cave_o_idx[reward.y][reward.x]);
    if(claimed) {
    /* The ownership/history scenarios start after the player has exposed the
     * upper reward. Wet construction alone must not consume that award. */
    cave_vertical_state drained;cave_vertical_capture(&drained);
    for(int i=0;i<drained.basin_count;i++)if(drained.basins[i].depth==8) {
        drained.basins[i].volume=0;drained.basins[i].flags|=CAVE_VERTICAL_CHANGED;
    }
    assert(cave_vertical_restore(&drained));cave_vertical_refresh();
    assert(vertical_vault_get()->placements[0].reward_layers==1);
    assert(cave_feat[reward.y][reward.x]==FEAT_FLOOR);
    int rewards=o_cnt;cave_vertical_refresh();assert(o_cnt==rewards);
    }
    assert(cave_vertical_get()->portal_count>=2);
    vertical_vault_visit(10,10);
    assert(vertical_vault_get()->placements[0].discovered_layers==1);
}

static void build_upper(void) { build_upper_state(true); }

static void assert_reciprocal(const cave_vertical_state *from,
    const cave_vertical_state *to,int source_depth,int target_depth)
{
    for(int i=0;i<from->portal_count;i++) {
        const cave_vertical_portal *a=&from->portals[i];
        if(a->peer_depth!=target_depth||a->kind==CAVE_VERTICAL_CONTROL)continue;
        bool found=false;
        for(int j=0;j<to->portal_count;j++) {
            const cave_vertical_portal *b=&to->portals[j];
            if(a->identity!=b->identity||b->peer_depth!=source_depth)continue;
            if(b->y!=a->peer_y||b->x!=a->peer_x)continue;
            assert(b->peer_y==a->y&&b->peer_x==a->x);
            assert(memcmp(b->local_patch,a->peer_patch,sizeof(b->local_patch))==0);
            assert(memcmp(b->peer_patch,a->local_patch,sizeof(b->peer_patch))==0);
            found=true;
        }
        assert(found);
    }
}

static void inherited_worker(int depth)
{
    memset(&next_job,0,sizeof(next_job));next_job.depth=depth;
    prepare_links(&next_job);
    fresh(depth);worker_job=&next_job;
    level_cache_generation_reserve();
    assert(vertical_vault_place());
    assert(cave_vertical_generation_finish());
}

static void test_cached_stair_and_long_fall_links(void)
{
    int failures=0;
    /* A four-floor fall must reserve its bottom even though it lies beyond
     * the usual adjacent-floor/shaft input window. */
    fresh(8);cave_set_feat(70,70,FEAT_CHASM);
    level_image *upper=capture();upper->chasm_drop[70][70]=4;
    floors[0]=active=upper;running=true;
    memset(&next_job,0,sizeof(next_job));next_job.depth=12;
    prepare_links(&next_job);
    bool bottom_reserved=next_job.links.feat[70][70]==FEAT_FLOOR
        &&next_job.links.ceiling[70][70]
        &&next_job.links.minimum_hgt>=72&&next_job.links.minimum_wid>=72;
    fresh(12);cave_set_feat(70,70,FEAT_WALL_EXTRA);
    worker_job=&next_job;
    bool finalized=level_cache_generation_links();
    bool bottom_placed=finalized&&cave_feat[70][70]==FEAT_FLOOR;
    worker_job=NULL;
    if(!bottom_reserved||!bottom_placed) {
        fprintf(stderr,"FAIL long fall: depth 8 drop 4 at 70,70; reserved=%d placed=%d link=%u ceiling=%u\n",
            bottom_reserved,bottom_placed,next_job.links.feat[70][70],next_job.links.ceiling[70][70]);
        failures++;
    }
    level_cache_clear();

    /* Shaft destination 10 is generated from active 8 while 9 is already
     * retained. Both existing floors own their reciprocal stair columns. */
    fresh(8);cave_set_feat(20,20,FEAT_MORE_SHAFT);
    upper=capture();floors[0]=active=upper;running=true;
    fresh(9);cave_set_feat(30,30,FEAT_MORE);
    level_image *middle=capture();floors[1]=middle;
    memset(&next_job,0,sizeof(next_job));next_job.depth=10;
    prepare_links(&next_job);
    assert(next_job.links.feat[20][20]==FEAT_LESS_SHAFT);
    assert(next_job.links.feat[30][30]==FEAT_LESS);
    fresh(10);
    /* These are ordinary generator-authored stairs with no peer at their
     * coordinate on either retained map. */
    cave_set_feat(40,40,FEAT_LESS);
    cave_set_feat(50,50,FEAT_LESS_SHAFT);
    worker_job=&next_job;
    finalized=level_cache_generation_links();worker_job=NULL;
    bool reciprocal=finalized&&cave_feat[20][20]==FEAT_LESS_SHAFT
        &&cave_feat[30][30]==FEAT_LESS
        &&cave_feat[40][40]!=FEAT_LESS
        &&cave_feat[50][50]!=FEAT_LESS_SHAFT;
    if(!reciprocal) {
        fprintf(stderr,"FAIL retained stair peer: active=8 retained=9 destination=10; unmatched up=%u unmatched shaft=%u finalized=%d\n",
            cave_feat[40][40],cave_feat[50][50],finalized);
        failures++;
    }
    level_cache_clear();
    assert(!failures);
    puts("Production cache links: four-floor chasm landing column and reciprocal stairs for every retained neighbor PASS.");
}

static void inherited_isolated_hole(void)
{
    fresh(8);cave_set_feat(20,20,FEAT_CHASM);
    level_image *upper=capture();upper->chasm_drop[20][20]=2;
    floors[0]=active=upper;running=true;
    memset(&next_job,0,sizeof(next_job));next_job.depth=9;
    prepare_links(&next_job);
    fresh(9);dun->cent_n=0;cave_set_feat(6,6,FEAT_MORE);
    for(int y=18;y<=22;y++)for(int x=18;x<=22;x++)
        cave_set_feat(y,x,FEAT_WALL_EXTRA);
    worker_job=&next_job;
    assert(level_cache_generation_links());
    assert(cave_feat[20][20]==FEAT_CHASM&&next_job.links.drop[20][20]==1);
}

static void test_inherited_hole_connectivity(void)
{
    int failures=0;
    inherited_isolated_hole();
    bool accepted=check_connectivity();
    bool preserved=cave_feat[20][20]==FEAT_CHASM;
    if(!accepted||!preserved) {
        fprintf(stderr,"FAIL isolated inherited hole: accepted=%d preserved=%d\n",accepted,preserved);
        failures++;
    }
    worker_job=NULL;level_cache_clear();

    inherited_isolated_hole();
    /* A hole used as a room center must not mask genuinely disconnected
     * dry/rubble components which still require ordinary rescue tunnels. */
    for(int y=28;y<=32;y++)for(int x=28;x<=32;x++)
        cave_set_feat(y,x,FEAT_WALL_EXTRA);
    cave_set_feat(30,30,FEAT_FLOOR);
    for(int y=38;y<=42;y++)for(int x=38;x<=42;x++)
        cave_set_feat(y,x,FEAT_WALL_EXTRA);
    cave_set_feat(40,40,FEAT_RUBBLE);
    dun->cent_n=2;dun->cent[0].y=dun->cent[0].x=20;
    dun->cent[1].y=dun->cent[1].x=30;
    accepted=check_connectivity();
    int access[MAX_DUNGEON_HGT][MAX_DUNGEON_WID]={{0}};
    flood_access(p_ptr->py,p_ptr->px,access,true);
    bool rescued=access[30][30]&&access[40][40];
    preserved=cave_feat[20][20]==FEAT_CHASM;
    if(!accepted||!rescued||!preserved) {
        fprintf(stderr,"FAIL hole room center plus dry/rubble islands: accepted=%d rescued=%d preserved=%d\n",
            accepted,rescued,preserved);failures++;
    }
    worker_job=NULL;level_cache_clear();

    fresh(9);dun->cent_n=0;
    for(int y=18;y<=22;y++)for(int x=18;x<=22;x++)
        cave_set_feat(y,x,FEAT_WALL_PERM);
    cave_set_feat(20,20,FEAT_MORE);
    if(check_connectivity()) {
        fprintf(stderr,"FAIL permanently sealed dry stair accepted\n");failures++;
    }
    assert(!failures);
    puts("Production inherited-hole connectivity: isolated holes preserved, hole room centers skipped, dry/rubble islands rescued, permanently sealed dry stairs rejected PASS.");
}

static void test_generation_upkeep_deferral(void)
{
    int arrow=0,failures=0;
    for(int i=1;i<z_info->k_max;i++)if(k_info[i].tval==TV_ARROW){arrow=i;break;}
    assert(arrow);
    upkeep_probe=true;
    for(int scene=0;scene<3;scene++) {
        fresh(9);character_generated=true;character_icky=0;
        character_dungeon=scene!=0;
        if(scene==0)p_ptr->py=p_ptr->px=0;
        if(scene==1)worker_job=&next_job;
        view_n=temp_n=0;
        upkeep_view_calls=upkeep_lore_calls=upkeep_quest_calls=0;
        p_ptr->update=PU_UPDATE_VIEW;
        object_type arrow_object;object_prep(&arrow_object,arrow);arrow_object.number=1;
        int placed=drop_near(&arrow_object,0,25,25);
        bool deferred=scene<2;
        bool correct=placed>0&&o_list[placed].k_idx==arrow
            &&(deferred ? ((p_ptr->update&PU_UPDATE_VIEW)
                &&!upkeep_view_calls&&!upkeep_lore_calls&&!upkeep_quest_calls&&view_n==0)
            : (!(p_ptr->update&PU_UPDATE_VIEW)&&upkeep_view_calls==1
                &&upkeep_lore_calls==1&&upkeep_quest_calls==1&&view_n>0));
        if(!correct) {
            fprintf(stderr,"FAIL drop upkeep scene=%d placed=%d flags=%u view=%d lore=%d quests=%d view_n=%d\n",
                scene,placed,p_ptr->update,upkeep_view_calls,upkeep_lore_calls,upkeep_quest_calls,view_n);
            failures++;
        }
        worker_job=NULL;
    }
    upkeep_probe=false;
    assert(!failures);
    puts("Production drop_near upkeep: incomplete map at 0,0 and complete worker defer view/lore/quests without losing flags; ready main map rebuilds real view PASS.");
}

static void test_natural_passage_generation(void)
{
    fresh(8);
    cave_vertical_state source={0};
    source.portal_count=1;source.breath=12;source.last_turn=turn;
    cave_vertical_portal *portal=&source.portals[0];
    *portal=(cave_vertical_portal){.identity=0x71550001,.y=20,.x=20,
        .peer_depth=9,.peer_y=24,.peer_x=24,.kind=CAVE_VERTICAL_RAPIDS,
        .material=FEAT_WATER,.strength=2,.flags=CAVE_VERTICAL_DOWN};
    for(int y=0;y<7;y++)for(int x=0;x<7;x++) {
        portal->local_patch[y][x]=FEAT_WATER;
        portal->peer_patch[y][x]=(ABS(y-3)+ABS(x-3)<=1)
            ?FEAT_DEEP_WATER:FEAT_FLOOR;
    }
    assert(cave_vertical_restore(&source));
    level_image *upper=capture();floors[0]=active=upper;running=true;
    memset(&next_job,0,sizeof(next_job));next_job.depth=9;
    prepare_links(&next_job);next_job.allow_down=false;
    fresh(9);worker_job=&next_job;
    level_cache_generation_reserve();
    assert(cave_vertical_get()->portals[0].local_patch[3][3]==FEAT_DEEP_WATER);
    /* The room planner may still carve/set bedrock in a reserved footprint;
     * those temporary writes must not rewrite its inherited blueprint. */
    cave_set_feat(24,24,FEAT_WALL_OUTER);
    if(cave_vertical_get()->portals[0].local_patch[3][3]!=FEAT_DEEP_WATER) {
        fputs("FAIL: worker layout overwrote natural receiving-pool blueprint\n",stderr);
        exit(1);
    }
    assert(cave_vertical_generation_finish());
    assert(cave_feat[24][24]==FEAT_DEEP_WATER&&cave_floor_bold(24,24));
    worker_job=NULL;character_dungeon=true;
    cave_set_feat(24,24,FEAT_ICE);
    assert(cave_vertical_get()->portals[0].local_patch[3][3]==FEAT_ICE);
    level_cache_clear();
    puts("Natural passage generation: worker room writes preserve inherited receiving pool; final placement is passable; live freezing still updates geometry PASS.");

    /* A later physical obstruction on a real resident receiver must refuse
     * passage before charging travel/impact, rather than using its spawn. */
    fresh(9);cave_set_feat(24,24,FEAT_WALL_EXTRA);
    level_image *lower=capture();floors[1]=lower;
    fresh(8);cave_feat[20][20]=FEAT_WATER;
    assert(cave_vertical_restore(&source));
    floors[0]=active=capture();running=true;character_dungeon=true;
    cave_m_idx[p_ptr->py][p_ptr->px]=0;
    p_ptr->py=p_ptr->px=20;cave_m_idx[20][20]=-1;
    p_ptr->energy_use=0;
    assert(cave_vertical_travel(false));
    if(p_ptr->depth!=8||p_ptr->leaving||p_ptr->energy_use||p_ptr->chp!=100) {
        fputs("FAIL: blocked receiving passage committed travel or charged energy\n",stderr);
        exit(1);
    }
    level_cache_clear();
    puts("Physical blocked receiver: ordinary passage stays on its source floor, with no cost or unrelated spawn fallback PASS.");
}

static void test_neighbor_reconstruction(void)
{
    build_upper();
    cave_set_feat(65,65,FEAT_RUBBLE);
    level_image *upper=capture();
    floors[0]=active=upper;running=true;
    inherited_worker(9);
    assert(next_job.inherited_count==1);
    assert_reciprocal(&upper->vertical,cave_vertical_get(),8,9);
    assert(vertical_vault_get()->placements[0].reward_layers==3);
    assert(vertical_vault_get()->placements[0].discovered_layers==1);
    vertical_vault_visit(10,10);
    assert(vertical_vault_get()->placements[0].discovered_layers==3);
    /* The production finalizer must accept matching authored stairs. */
    assert(level_cache_generation_links());
    worker_job=NULL;
    level_image *lower=capture();floors[1]=lower;
    fresh(10);floors[2]=active=capture();
    requested_depth=pending_depth=-1;
    prune_cache();
    assert(!find_floor(8)&&find_floor(9)&&find_floor(10));
    assert(!level_cache_retains(8)&&level_cache_retains(9)&&level_cache_retains(10));
    inherited_worker(8);
    assert(!next_job.allow_down&&next_job.inherited_count==1);
    assert(vertical_vault_get()->count==1);
    assert(vertical_vault_get()->placements[0].reward_layers==3);
    assert(vertical_vault_get()->placements[0].discovered_layers==3);
    assert(cave_feat[65][65]!=FEAT_RUBBLE);
    assert(level_cache_generation_links());
    assert_reciprocal(&lower->vertical,cave_vertical_get(),9,8);
    worker_job=NULL;
    /* None of the evicted upper's unrelated cells are worker input. */
    assert(next_job.links.feat[65][65]==0);
    level_cache_clear();
    puts("Real cache prune/prepare_links + builder: eviction, neighbor-only frame reconstruction, fresh surrounding map, reciprocal endpoints, discovery/reward inheritance PASS.");
}

static void decode(const byte *in,byte *out,size_t length)
{
    byte prev=0;for(size_t i=0;i<length;i++){out[i]=in[i]^prev;prev=in[i];}
}
static void encode(const byte *in,byte *out,size_t length)
{
    byte prev=0;for(size_t i=0;i<length;i++){out[i]=in[i]^prev;prev=out[i];}
}
static bool read_region_bytes(const byte *bytes,size_t length,level_image *out)
{
    out->depth=p_ptr->depth;out->hgt=out->wid=88;
    fixture_read_begin(bytes,length,VERSION_EXTRA);
    bool ok=read_regions(out);fixture_read_end();return ok;
}
static void bad_region(size_t length,size_t offset,byte value)
{
    byte old=plain[offset];plain[offset]=value;encode(plain,altered,length);
    level_image *out=level_image_new();assert(out);
    assert(!read_region_bytes(altered,length,out));
    level_image_free(out);plain[offset]=old;
}

static void test_regions_save(void)
{
    build_upper();level_image *image=capture();
    fixture_write_begin(encoded,sizeof(encoded));write_regions(image);
    save_wr_u32b(0x1928ABCDU);size_t length=fixture_write_end();
    level_image *out=level_image_new();assert(out);out->depth=8;out->hgt=out->wid=88;
    fixture_read_begin(encoded,length,VERSION_EXTRA);
    assert(read_regions(out));u32b sentinel;load_rd_u32b(&sentinel);
    assert(sentinel==0x1928ABCDU&&fixture_read_end()==length);
    assert(memcmp(&out->vertical,&image->vertical,sizeof(image->vertical))==0);
    assert(memcmp(&out->vertical_vaults,&image->vertical_vaults,sizeof(image->vertical_vaults))==0);
    decode(encoded,plain,length);
    size_t portal_start=8,portal_record=28+2*CAVE_VERTICAL_PATCH*CAVE_VERTICAL_PATCH;
    size_t basin_start=portal_start+portal_record*image->vertical.portal_count;
    size_t vault_start=basin_start+20*image->vertical.basin_count;
    assert(plain[vault_start]==1);
    bad_region(length,0,CAVE_VERTICAL_PORTALS+1);
    bad_region(length,1,CAVE_VERTICAL_BASINS+1);
    bad_region(length,2,13);bad_region(length,3,2);
    bad_region(length,portal_start+14,0); /* invalid kind */
    bad_region(length,portal_start+15,255); /* invalid material */
    bad_region(length,portal_start+16,4); /* invalid strength */
    bad_region(length,portal_start+17,10); /* invalid direction */
    bad_region(length,portal_start+18,64); /* outside defined portal flags */
    bad_region(length,portal_start+19,255); /* invalid in-flight material */
    bad_region(length,portal_start+23,17); /* pending exceeds 4096 */
    bad_region(length,portal_start+27,255); /* negative due turn */
    bad_region(length,portal_start+28,255); /* invalid patch feature */
    bad_region(length,basin_start+17,9); /* outside authored basin radius */
    bad_region(length,basin_start+16,255); /* invalid basin material */
    bad_region(length,basin_start+18,7); /* invalid hot duration */
    bad_region(length,basin_start+19,4); /* undefined basin flag */
    bad_region(length,basin_start+11,1); /* volume exceeds capacity */
    bad_region(length,vault_start,VERTICAL_VAULT_MAX+1);
    bad_region(length,vault_start+1+14,8); /* invalid transform */
    bad_region(length,vault_start+1+15,4); /* invalid discovery bits */
    bad_region(length,vault_start+1+16,4); /* invalid reward bits */
    /* Truncated data must be rejected, not accepted with zero-filled fields. */
    assert(!read_region_bytes(encoded,length-8,out));
    level_image_free(out);level_image_free(image);
    puts("Real region writers/readers: field roundtrip, trailing sentinel, malformed counts/kind/strength/flags/patch/radius/vault bits and truncation PASS.");
}

static void test_v5_metadata(void)
{
    build_upper();level_image *image=capture();
    fixture_write_begin(encoded,sizeof(encoded));write_vertical(image);
    size_t metadata=fixture_write_end();
    fixture_write_begin(altered,sizeof(altered));write_regions(image);
    size_t regions=fixture_write_end();
    assert(metadata>regions);
    /* v5 metadata is byte-identical through hint metadata and has no v6
     * region suffix. Preserve its following field at the exact boundary. */
    decode(encoded,plain,metadata);
    size_t v5_length=metadata-regions;
    plain[v5_length]=0xCD;plain[v5_length+1]=0xAB;
    plain[v5_length+2]=0x28;plain[v5_length+3]=0x19;
    encode(plain,altered,v5_length+4);
    level_image *out=level_image_new();assert(out);out->depth=8;out->hgt=out->wid=88;
    out->vertical.portal_count=23;out->vertical_vaults.count=4;
    fixture_read_begin(altered,v5_length+4,5);
    assert(read_vertical(out));u32b sentinel;load_rd_u32b(&sentinel);
    assert(sentinel==0x1928ABCDU&&fixture_read_end()==v5_length+4);
    assert(out->vertical.portal_count==0&&out->vertical.basin_count==0);
    assert(out->vertical.breath==12&&out->vertical.last_turn==turn);
    assert(out->vertical_vaults.count==0);
    level_image_free(out);level_image_free(image);
    puts("Real v5 cache metadata migration: absent v6 region suffix defaults safely, legacy prefix and following sentinel preserved PASS.");
}

static void test_public_cache_save(void)
{
    fresh(7);floors[0]=capture();
    build_upper();floors[1]=active=capture();running=true;
    inherited_worker(9);worker_job=NULL;floors[2]=capture();
    level_image_restore(active);
    fixture_write_begin(encoded,sizeof(encoded));level_cache_write();
    save_wr_u32b(0x1928ABCDU);size_t length=fixture_write_end();
    level_image *current=level_image_new();assert(current);level_image_capture(current);
    level_cache_clear();level_image_restore(current);
    fixture_read_begin(encoded,length,VERSION_EXTRA);
    assert(level_cache_read()==0);u32b sentinel;load_rd_u32b(&sentinel);
    assert(sentinel==0x1928ABCDU&&fixture_read_end()==length);
    assert(active&&active->depth==8&&find_floor(7)&&find_floor(9));
    assert(!find_floor(6)&&!find_floor(10));
    assert(active->vertical_vaults.count==1&&find_floor(9)->vertical_vaults.count==1);
    assert(active->vertical_vaults.placements[0].reward_layers==3);
    assert(find_floor(9)->vertical_vaults.placements[0].reward_layers==3);
    assert_reciprocal(&active->vertical,&find_floor(9)->vertical,8,9);
    level_cache_clear();level_image_free(current);
    puts("Public level_cache_write/read: active plus both resident neighbors, real dungeon lanes, region/vault state and following sentinel PASS.");
}

static void test_public_v5_cache(void)
{
    static byte legacy_metadata[3][65536];static size_t sizes[3];
    fresh(7);floors[0]=capture();
    build_upper();floors[1]=active=capture();running=true;
    inherited_worker(9);worker_job=NULL;floors[2]=capture();
    level_image_restore(active);
    /* Construct the historical cache lane using unchanged production writers
     * for each field, omitting precisely the version6 region suffix. */
    for(int i=0;i<3;i++) {
        fixture_write_begin(encoded,sizeof(encoded));write_vertical(floors[i]);
        size_t full=fixture_write_end();
        fixture_write_begin(altered,sizeof(altered));write_regions(floors[i]);
        size_t suffix=fixture_write_end();assert(full>suffix);
        sizes[i]=full-suffix;assert(sizes[i]<sizeof(legacy_metadata[i]));
        decode(encoded,legacy_metadata[i],sizes[i]);
    }
    fixture_write_begin(encoded,sizeof(encoded));
    save_wr_u16b(LEVEL_CACHE_SAVE_MAGIC);save_wr_byte(1);
    save_wr_s16b(-1);save_wr_s16b(0);save_wr_s16b(0);save_wr_byte(0);
    for(size_t j=0;j<sizes[1];j++)save_wr_byte(legacy_metadata[1][j]);
    save_wr_byte(2);
    fixture_set_legacy_monsters(true);
    for(int i=0;i<3;i++)if(i!=1) {
        level_image_restore(floors[i]);
        save_write_partition_meta();save_write_dungeon();
        for(size_t j=0;j<sizes[i];j++)save_wr_byte(legacy_metadata[i][j]);
    }
    fixture_set_legacy_monsters(false);
    save_wr_u32b(0x1928ABCDU);save_wr_u32b(0x76543210U);
    size_t length=fixture_write_end();
    level_image_restore(active);
    level_image *current=capture();level_cache_clear();level_image_restore(current);
    fixture_read_begin(encoded,length,5);
    assert(level_cache_read()==0);u32b sentinel,tail;
    load_rd_u32b(&sentinel);load_rd_u32b(&tail);
    assert(sentinel==0x1928ABCDU&&tail==0x76543210U&&fixture_read_end()==length);
    assert(active->depth==8&&find_floor(7)&&find_floor(9));
    for(int i=0;i<3;i++) {
        assert(floors[i]->vertical.portal_count==0&&floors[i]->vertical.basin_count==0);
        assert(floors[i]->vertical.breath==12&&floors[i]->vertical_vaults.count==0);
    }
    level_cache_clear();level_image_free(current);
    puts("Public cache reader over synthesized exact v5 layout: active plus two real dungeon neighbors, absent region/vault defaults, following two-word tail PASS.");
}

static Uint32 SDLCALL generation_deadline(void *data,SDL_TimerID id,Uint32 interval)
{
    (void)id;(void)interval;SDL_SetAtomicInt(data,1);return 0;
}

static level_image *seeded_neighbor(int depth,u32b seed)
{
    memset(&next_job,0,sizeof(next_job));next_job.depth=depth;
    prepare_links(&next_job);
    bool baseline=baseline_probe&&depth==10;
    byte original_type=v_info[525].typ;
    if(baseline) {
        memset(&next_job.links,0,sizeof(next_job.links));next_job.links.source_depth=-1;
        next_job.inherited_count=0;next_job.allow_down=false;
        v_info[525].typ=0; /* in-memory baseline excludes only paired lottery */
    }
    player_type before=*p_ptr;
    byte *unique_counts=SDL_calloc(z_info->r_max,sizeof(*unique_counts));assert(unique_counts);
    for(int i=1;i<z_info->r_max;i++)if(r_info[i].flags1&RF1_UNIQUE)unique_counts[i]=r_info[i].cur_num;
    fresh(depth);
    for(int i=1;i<z_info->r_max;i++)if(r_info[i].flags1&RF1_UNIQUE)r_info[i].cur_num=unique_counts[i];
    SDL_free(unique_counts);
    *p_ptr=before;p_ptr->depth=depth;p_ptr->create_stair=p_ptr->create_rubble=0;
    next_job.image=level_image_new();assert(next_job.image);
    next_job.before=before;
    worker_job=&next_job;
    memset(failure_counts,0,sizeof(failure_counts));
    Rand_state_init(seed);
    SDL_TimerID deadline=SDL_AddTimer(30000,generation_deadline,&next_job.cancel);
    assert(deadline);
    printf("Seeded production generate_cave: depth=%d seed=%u\n",depth,(unsigned)seed);
    if(getenv("SIL_VERTICAL_TRACE"))log_set_level(LOG_TRACE);
    generate_cave();SDL_RemoveTimer(deadline);
    v_info[525].typ=original_type;
    if(SDL_GetAtomicInt(&next_job.cancel)) {
        printf("Real failure counts links/vertical/connectivity/player/history: %u/%u/%u/%u/%u\n",
            failure_counts[0],failure_counts[1],failure_counts[2],failure_counts[3],failure_counts[4]);
        printf("Generation deadline: depth=%d portals=%u pairs=%u links_ok=%d\n",depth,
            cave_vertical_get()->portal_count,vertical_vault_get()->count,level_cache_generation_links());
        int (*seen)[MAX_DUNGEON_WID]=SDL_calloc(MAX_DUNGEON_HGT,sizeof(*seen));
        assert(seen);flood_access(p_ptr->py,p_ptr->px,seen,false);
        int reported=0;
        for(int y=1;y<p_ptr->cur_map_hgt-1&&reported<10;y++)
            for(int x=1;x<p_ptr->cur_map_wid-1&&reported<10;x++)
                if(!seen[y][x]&&cave_floor_bold(y,x)&&player_passable(y,x,false)) {
                    printf("Unreachable passable floor: %d,%d feat=%u info=%u N/S/W/E=%u/%u/%u/%u link=%u\n",
                        y,x,cave_feat[y][x],cave_info[y][x],cave_feat[y-1][x],cave_feat[y+1][x],cave_feat[y][x-1],cave_feat[y][x+1],next_job.links.feat[y][x]);
                    reported++;
                }
        SDL_free(seen);
        for(int i=0;i<cave_vertical_get()->portal_count;i++) {
            const cave_vertical_portal *p=&cave_vertical_get()->portals[i];
            for(int dy=-3;dy<=3;dy++)for(int dx=-3;dx<=3;dx++) {
                int y=p->y+dy,x=p->x+dx;
                if(!p->local_patch[dy+3][dx+3]||!in_bounds_fully(y,x))continue;
                if((cave_info[y][x]&CAVE_ICKY)||cave_stair_bold(y,x)||cave_forge_bold(y,x)
                    ||cave_o_idx[y][x]||cave_m_idx[y][x]>0)
                    printf("Inherited patch conflict: portal=%d kind=%u origin=%d,%d cell=%d,%d feat=%u info=%u object=%d monster=%d link=%u\n",
                        i,p->kind,p->y,p->x,y,x,cave_feat[y][x],cave_info[y][x],cave_o_idx[y][x],cave_m_idx[y][x],next_job.links.feat[y][x]);
            }
        }
    }
    assert(!SDL_GetAtomicInt(&next_job.cancel)&&character_dungeon);
    level_cache_generation_finished();
    level_image_capture(next_job.image);next_job.image->visited=true;
    worker_job=NULL;return next_job.image;
}

static u32b terrain_hash(const level_image *image)
{
    u32b hash=2166136261U;
    for(int y=0;y<image->hgt;y++)for(int x=0;x<image->wid;x++)
        hash=(hash^image->feat[y][x])*16777619U;
    return hash;
}

static void assert_unique_reservations(const level_image *extra)
{
    for(int race=1;race<z_info->r_max;race++)if(r_info[race].flags1&RF1_UNIQUE) {
        int count=0;
        for(int slot=0;slot<LEVEL_CACHE_SLOTS;slot++)if(floors[slot])
            for(int i=1;i<floors[slot]->mon_max;i++)count+=floors[slot]->monsters[i].r_idx==race;
        if(extra)for(int i=1;i<extra->mon_max;i++)count+=extra->monsters[i].r_idx==race;
        assert(count<=1);
    }
}

static void test_seeded_generation(void)
{
    build_upper();floors[0]=active=capture();running=true;
    u32b original_hash=terrain_hash(active);
    level_image *lower=seeded_neighbor(9,1101);
    floors[1]=active=lower;
    /* The retained lower half may also start a different pair down to10. */
    assert(lower->vertical_vaults.count>=1&&lower->vertical_vaults.count<=VERTICAL_VAULT_MAX);
    assert(lower->vertical_vaults.placements[0].identity==blueprint().placements[0].identity);
    assert(lower->vertical_vaults.placements[0].reward_layers==3);
    assert_reciprocal(&floors[0]->vertical,&lower->vertical,8,9);
    assert_unique_reservations(NULL);
    level_image *deeper=seeded_neighbor(10,2202);
    floors[2]=active=deeper;requested_depth=pending_depth=-1;
    assert_unique_reservations(NULL);
    prune_cache();assert(!find_floor(8)&&find_floor(9)&&find_floor(10));
    level_image *rebuilt=seeded_neighbor(8,3303);
    assert(rebuilt->vertical_vaults.count==1);
    assert(rebuilt->vertical_vaults.placements[0].identity==blueprint().placements[0].identity);
    assert(rebuilt->vertical_vaults.placements[0].reward_layers==3);
    assert_reciprocal(&lower->vertical,&rebuilt->vertical,9,8);
    assert(terrain_hash(rebuilt)!=original_hash);
    assert_unique_reservations(rebuilt);
    level_image_free(rebuilt);level_cache_clear();
    puts("Three seeded production generate_cave runs: depths9/10/8, evicted upper regenerated from lower blueprint, aligned endpoints, reward history and fresh surrounding terrain PASS.");
}

static void test_generation_baseline(void)
{
    baseline_probe=true;
    build_upper();floors[0]=active=capture();running=true;
    level_image *lower=seeded_neighbor(9,1101);floors[1]=active=lower;
    level_image *probe=seeded_neighbor(10,2202);level_image_free(probe);
    level_cache_clear();baseline_probe=false;
    puts("Seed2202 baseline: same player/unique state, no inherited links/regions/blueprints, allow_down=false, paired lottery disabled only in memory PASS.");
}

static void test_remote_transactions(void)
{
    fresh(9);
    int arrow=0;
    for(int i=1;i<z_info->k_max;i++)if(k_info[i].tval==TV_ARROW){arrow=i;break;}
    assert(arrow);
    object_type stack;object_prep(&stack,arrow);stack.number=2;
    int existing=floor_carry(25,25,&stack);assert(existing);
    assert(place_monster_one(20,20,31,false,false,NULL));
    int monster=cave_m_idx[20][20];assert(monster>0);
    mon_list[monster].hp=mon_list[monster].maxhp=10;
    assert(place_monster_one(20,21,31,false,false,NULL));
    int rival=cave_m_idx[20][21];assert(rival>0);
    assert(monster_social_set_group(&mon_list[monster],8));
    assert(monster_social_set_group(&mon_list[rival],9));
    assert(monster_social_start_feud(monster,rival));
    mon_list[rival].social_player_threat=2;
    assert(monster_group_set_relation(10,11,MON_REL_HOSTILE));
    level_image *remote=capture();floors[1]=remote;
    fresh(8);
    assert(place_monster_one(21,20,31,false,false,NULL));
    assert(place_monster_one(21,21,31,false,false,NULL));
    int home=cave_m_idx[21][20],home_rival=cave_m_idx[21][21];
    assert(monster_social_set_group(&mon_list[home],8));
    assert(monster_social_set_group(&mon_list[home_rival],9));
    assert(monster_social_start_feud(home,home_rival));
    mon_list[home].social_cooldown=37;mon_list[home].social_player_threat=1;
    assert(monster_group_set_relation(10,11,MON_REL_NEUTRAL));
    monster_type home_before=mon_list[home],home_rival_before=mon_list[home_rival];
    floors[0]=active=capture();running=true;
    object_prep(&stack,arrow);stack.number=5;stack.pval=7;
    int source=floor_carry(24,24,&stack);assert(source);
    o_list[source].marked=true;
    o_list[source].obj_note=quark_add("transport fixture");
    object_type expected=o_list[source];
    int before_count=o_cnt;
    assert(level_cache_transfer_object(9,source,25,25));
    assert(!o_list[source].k_idx&&!cave_o_idx[24][24]&&o_cnt==before_count-1);
    int moved=remote->objects_at[25][25];assert(moved!=existing);
    expected.iy=expected.ix=25;expected.next_o_idx=existing;
    assert(!memcmp(&remote->objects[moved],&expected,sizeof(expected)));
    assert(remote->objects[moved].number+remote->objects[existing].number==7);
    assert(remote->o_cnt==2);
    object_prep(&stack,arrow);stack.number=3;
    source=floor_carry(24,24,&stack);assert(source);
    expected=o_list[source];before_count=o_cnt;
    int target_head=remote->objects_at[25][25];
    remote->o_max=z_info->o_max;remote->o_cnt=z_info->o_max-1;
    for(int i=1;i<remote->o_max;i++)remote->objects[i].k_idx=arrow;
    assert(!level_cache_transfer_object(9,source,25,25));
    assert(!memcmp(&o_list[source],&expected,sizeof(expected))&&o_cnt==before_count);
    assert(remote->objects_at[25][25]==target_head);
    /* Restore unused remote slots before canonical monster death drops. */
    for(int i=3;i<remote->o_max;i++)memset(&remote->objects[i],0,sizeof(remote->objects[i]));
    remote->o_max=3;remote->o_cnt=2;
    p_ptr->target_set=true;p_ptr->target_who=0;p_ptr->target_row=7;p_ptr->target_col=8;
    p_ptr->health_who=0;p_ptr->energy=73;p_ptr->energy_use=100;
    int py=p_ptr->py,px=p_ptr->px,depth=p_ptr->depth;
    u32b map=terrain_hash(active);
    s32b xp=p_ptr->exp;int kills=l_list[31].pkills;
    assert(!level_cache_damage_monster(9,monster,3,"",-1));
    assert(remote->monsters[monster].hp==7);
    assert(remote->monsters[monster].social_group==8);
    assert(remote->monsters[monster].social_rival==rival);
    assert(remote->monsters[rival].social_rival==monster);
    assert(remote->monsters[rival].social_player_threat==2);
    assert(remote->relations[10][11]==MON_REL_HOSTILE);
    assert(!memcmp(&mon_list[home],&home_before,sizeof(home_before)));
    assert(!memcmp(&mon_list[home_rival],&home_rival_before,sizeof(home_rival_before)));
    assert(monster_social_valid(&mon_list[home])&&monster_social_valid(&mon_list[home_rival]));
    assert(monster_group_relation(10,11)==MON_REL_NEUTRAL);
    assert(p_ptr->depth==depth&&p_ptr->py==py&&p_ptr->px==px&&p_ptr->energy==73&&p_ptr->energy_use==100);
    assert(p_ptr->target_set&&p_ptr->target_row==7&&p_ptr->target_col==8);
    assert(p_ptr->update&(PU_FORGET_VIEW|PU_UPDATE_VIEW|PU_MONSTERS));
    assert(level_cache_damage_monster(9,monster,1000,"",-1));
    assert(!remote->monsters[monster].r_idx&&!remote->monsters_at[20][20]);
    assert(remote->monsters[rival].social_rival==0);
    assert(!memcmp(&mon_list[home],&home_before,sizeof(home_before)));
    assert(l_list[31].pkills==kills+1&&p_ptr->exp>xp);
    assert(p_ptr->depth==depth&&p_ptr->py==py&&p_ptr->px==px);
    assert(terrain_hash(active)==map);
    level_cache_clear();
    puts("Real cache object/monster transactions: exact marked stack conservation, capacity refusal, remote nonlethal/lethal canonical death+XP, active position/target/energy/terrain and both floors' personal feuds/diplomacy preserved PASS.");
}

static void test_aperture_visibility(void)
{
    fresh(9);cave_feat[20][20]=FEAT_DEEP_WATER;cave_info[20][20]=CAVE_GLOW;
    level_image *lower=capture();floors[1]=lower;
    fresh(8);character_dungeon=true;
    cave_feat[20][20]=FEAT_CHASM;cave_info[20][20]=CAVE_SEEN;
    floors[0]=active=capture();running=true;
    cave_vertical_view_info view;char description[256];
    assert(cave_vertical_view(20,20,&view));
    assert((view.flags&CAVE_VERTICAL_BELOW_VISIBLE)&&view.feature==FEAT_DEEP_WATER&&view.relative_depth==1);
    assert(cave_vertical_describe(20,20,description,sizeof(description))&&strstr(description,"50 ft below"));
    lower->feat[20][20]=FEAT_TRAP_PIT;lower->info[20][20]=CAVE_HIDDEN|CAVE_GLOW;
    assert(cave_vertical_view(20,20,&view)&&view.feature==FEAT_FLOOR);
    lower->info[20][20]=CAVE_GLOW;
    assert(cave_vertical_view(20,20,&view)&&view.feature==FEAT_TRAP_PIT);
    lower->feat[20][20]=FEAT_SECRET;assert(f_info[FEAT_SECRET].mimic);
    assert(cave_vertical_view(20,20,&view)&&view.feature==f_info[FEAT_SECRET].mimic);
    lower->info[20][20]=0;lower->light[20][20]=9;
    assert(!cave_vertical_view(20,20,&view));
    lower->info[20][20]=CAVE_GLOW;
    assert(cave_vertical_view(20,20,&view)&&(view.flags&CAVE_VERTICAL_BELOW_VISIBLE));
    lower->info[20][20]=0;p_ptr->cur_light=1;p_ptr->py=p_ptr->px=19;
    assert(cave_vertical_view(20,20,&view)&&(view.flags&CAVE_VERTICAL_BELOW_VISIBLE));
    p_ptr->py=p_ptr->px=5;
    assert(!cave_vertical_view(20,20,&view));
    lower->info[20][20]=CAVE_GLOW;
    p_ptr->blind=1;assert(!cave_vertical_view(20,20,&view));p_ptr->blind=0;
    p_ptr->image=1;assert(!cave_vertical_view(20,20,&view));p_ptr->image=0;
    cave_info[20][20]=CAVE_MARK;
    assert(!cave_vertical_view(20,20,&view));
    p_ptr->rage=1;g_labyrinth_view_active=true;
    assert(!cave_vertical_describe(20,20,description,sizeof(description)));
    p_ptr->rage=0;g_labyrinth_view_active=false;cave_info[20][20]=CAVE_SEEN;
    cave_feat[20][20]=FEAT_BRIDGE_CHASM_H;
    assert(cave_vertical_view(20,20,&view)&&(view.flags&CAVE_VERTICAL_BELOW_VISIBLE));
    assert(!level_cache_vertical_aperture(20,20,NULL,NULL,NULL));
    cave_feat[20][20]=FEAT_CHASM;
    cave_vertical_state state={0};state.breath=12;state.last_turn=turn;state.portal_count=1;
    state.portals[0]=(cave_vertical_portal){.identity=91,.y=20,.x=20,
        .peer_depth=9,.peer_y=24,.peer_x=24,.kind=CAVE_VERTICAL_FALL,
        .material=FEAT_WATER,.strength=3,.flags=CAVE_VERTICAL_DOWN};
    assert(cave_vertical_restore(&state));
    lower->feat[24][24]=FEAT_WATER;lower->info[24][24]=CAVE_GLOW;
    assert(cave_vertical_view(20,20,&view)&&view.feature==FEAT_WATER);
    int depth,y,x;assert(level_cache_vertical_aperture(20,20,&depth,&y,&x));
    assert(depth==9&&y==24&&x==24);
    s32b before_turn=turn;u32b before_hash=terrain_hash(lower);
    for(int i=0;i<20;i++)assert(cave_vertical_view(20,20,&view));
    assert(turn==before_turn&&terrain_hash(lower)==before_hash);
    cave_feat[20][20]=FEAT_WATER;p_ptr->energy_use=100;
    pending_depth=9;int health=p_ptr->chp;
    assert(cave_vertical_forced_travel(20,20));
    assert(cave_vertical_forced_travel(20,20));
    assert(p_ptr->chp==health&&p_ptr->depth==8&&p_ptr->energy_use==100);
    pending_depth=-1;
    p_ptr->playing=true;p_ptr->noscore=0;p_ptr->game_type=0;p_ptr->is_dead=false;
    p_ptr->chp=p_ptr->mhp=1000;
    sil_achievements_begin_run();
    assert(cave_vertical_forced_travel(20,20));
    assert(p_ptr->depth==9 && (p_ptr->achievement_flags&SIL_ACH_RUN_FALLING));
    assert(p_ptr->achievement_fall_depth==8);
    level_cache_clear();
    puts("Production aperture reader: lit/glowing/near-torch terrain, hidden trap and secret sanitizing, darkness/blindness/hallucination/restricted memory, bridge foreground, displaced endpoint, read-only sampling and pending-fall impact dedup PASS.");
}

static void test_packet_and_mirrors(void)
{
    build_upper();
    cave_vertical_state upper; cave_vertical_capture(&upper);
    int sender=-1;
    for(int i=0;i<upper.portal_count;i++)
        if(upper.portals[i].kind==CAVE_VERTICAL_FALL)sender=i;
    assert(sender>=0);
    u32b identity=upper.portals[sender].identity;
    upper.portals[sender].pending=7;upper.portals[sender].due=turn+20;
    for(int i=0;i<upper.basin_count;i++)if(upper.basins[i].depth==8)upper.basins[i].volume=43;
    assert(cave_vertical_restore(&upper));
    floors[0]=active=capture();running=true;
    inherited_worker(9);worker_job=NULL;
    cave_vertical_state lower;cave_vertical_capture(&lower);
    for(int i=0;i<lower.basin_count;i++) {
        if(lower.basins[i].depth==8&&lower.basins[i].identity==identity)lower.basins[i].volume=12;
        if(lower.basins[i].depth==9&&lower.basins[i].identity==identity)lower.basins[i].volume=55;
    }
    assert(cave_vertical_restore(&lower));
    floors[1]=active=capture();
    level_cache_process_vertical();
    cave_vertical_capture(&lower);
    for(int i=0;i<lower.basin_count;i++)if(lower.basins[i].depth==8&&lower.basins[i].identity==identity)assert(lower.basins[i].volume==43);
    for(int i=0;i<floors[0]->vertical.basin_count;i++)if(floors[0]->vertical.basins[i].depth==9&&floors[0]->vertical.basins[i].identity==identity)
        assert(floors[0]->vertical.basins[i].volume==55);
    int receiver=-1;
    for(int i=0;i<lower.portal_count;i++)if(lower.portals[i].identity==identity
        &&lower.portals[i].kind==CAVE_VERTICAL_FALL)receiver=i;
    assert(receiver>=0&&(lower.portals[receiver].flags&CAVE_VERTICAL_CLOSED));
    evict_floor(0);assert(!find_floor(8));cave_vertical_capture(&lower);
    assert(lower.portals[receiver].pending==7&&lower.portals[receiver].due==turn+20);
    turn+=20;cave_vertical_process();cave_vertical_capture(&lower);
    assert(!lower.portals[receiver].pending);
    for(int i=0;i<lower.basin_count;i++)if(lower.basins[i].depth==9&&lower.basins[i].identity==identity)assert(lower.basins[i].volume==62);
    turn+=20;cave_vertical_process();cave_vertical_capture(&lower);
    for(int i=0;i<lower.basin_count;i++)if(lower.basins[i].depth==9&&lower.basins[i].identity==identity)assert(lower.basins[i].volume==62);
    level_cache_clear();
    puts("Real cache basin mirrors/eviction: owning volumes synchronize, closed flag preserved, seven-unit pending packet survives sender eviction and credits exactly once PASS.");
}

static void test_shared_pair_history(void)
{
    build_upper();level_image *upper=capture();floors[0]=active=upper;running=true;
    inherited_worker(9);worker_job=NULL;
    vertical_vault_visit(10,10);
    floors[1]=active=capture();
    assert(active->vertical_vaults.placements[0].reward_layers==3);
    assert(active->vertical_vaults.placements[0].discovered_layers==3);
    level_cache_process_vertical();
    fresh(7);floors[2]=active=capture();
    requested_depth=pending_depth=-1;prune_cache();
    assert(!find_floor(9)&&find_floor(8));
    assert(upper->vertical_vaults.placements[0].reward_layers==3);
    assert(upper->vertical_vaults.placements[0].discovered_layers==3);
    inherited_worker(9);worker_job=NULL;
    assert(vertical_vault_get()->placements[0].reward_layers==3);
    assert(vertical_vault_get()->placements[0].discovered_layers==3);
    /* Transformed blueprint row18,col30 is the lower authored chest. */
    assert(cave_o_idx[38][12]==0);
    level_cache_clear();
    puts("Real opposite-direction eviction: lower chest/discovery history merges into retained upper, rebuilt lower cannot mint the chest again PASS.");
}

static void test_cold_tube_reconstruction(void)
{
    fresh(8);cave_vertical_state source={0};
    source.portal_count=1;source.basin_count=2;source.breath=12;source.last_turn=turn;
    source.portals[0]=(cave_vertical_portal){.identity=123,.y=20,.x=20,
        .peer_depth=9,.peer_y=24,.peer_x=24,.kind=CAVE_VERTICAL_TUBE,
        .material=FEAT_LAVA,.strength=3,.flags=CAVE_VERTICAL_DOWN|CAVE_VERTICAL_REVERSIBLE};
    for(int y=0;y<7;y++)for(int x=0;x<7;x++) {
        source.portals[0].local_patch[y][x]=FEAT_FLOOR;
        source.portals[0].peer_patch[y][x]=FEAT_LAVA;
    }
    source.basins[0]=(cave_vertical_basin){.identity=123,.depth=8,.y=20,.x=20,
        .capacity=144,.material=FEAT_LAVA,.radius=3,
        .flags=CAVE_VERTICAL_RESERVOIR|CAVE_VERTICAL_CHANGED};
    source.basins[1]=(cave_vertical_basin){.identity=123,.depth=9,.y=24,.x=24,
        .capacity=144,.material=FEAT_LAVA,.radius=3,
        .flags=CAVE_VERTICAL_SINK|CAVE_VERTICAL_CHANGED};
    fresh(9);cave_vertical_generation_begin(false);
    cave_vertical_generation_inherit(&source,8,9);
    assert(cave_vertical_generation_finish());
    /* No world tick may be required before the arriving player is safe. */
    assert(cave_feat[24][24]==FEAT_FLOOR);
    p_ptr->py=p_ptr->px=24;
    assert(cave_vertical_can_travel(true));
    assert(cave_vertical_get()->last_turn==turn);
    puts("Real cold-tube receiver generation: zero-volume cooled basin overrides stale lava patch before first entry, upstream passage immediately usable PASS.");
}

static void assert_authored_water_empty(const level_image *image)
{
    const vertical_vault_state *vaults=image?&image->vertical_vaults:vertical_vault_get();
    int depth=image?image->depth:p_ptr->depth, count=0;
    for(int y=0;y<88;y++)for(int x=0;x<88;x++) {
        int profile=vertical_vault_liquid_profile(vaults,depth,blueprint().placements[0].identity,y,x);
        if(profile<2)continue;
        int feature=image?image->feat[y][x]:cave_feat[y][x];
        bool dry_shaft=false;
        const cave_vertical_state *regions=image?&image->vertical:cave_vertical_get();
        for(int p=0;p<regions->portal_count;p++) {
            const cave_vertical_portal *portal=&regions->portals[p];
            dry_shaft|=feature==FEAT_CHASM&&portal->y==y&&portal->x==x
                &&(portal->flags&CAVE_VERTICAL_DOWN)
                &&(portal->kind==CAVE_VERTICAL_FALL||portal->kind==CAVE_VERTICAL_LAVA_FALL);
        }
        assert(feature==FEAT_FLOOR||dry_shaft);count++;
    }
    assert(count>100);
}

static void test_authored_pool_drain(void)
{
    build_upper();cave_vertical_state state;cave_vertical_capture(&state);
    for(int i=0;i<state.basin_count;i++)if(state.basins[i].depth==8) {
        state.basins[i].volume=0;state.basins[i].flags|=CAVE_VERTICAL_RESERVOIR|CAVE_VERTICAL_CHANGED;
    }
    assert(cave_vertical_restore(&state));cave_vertical_process();
    assert_authored_water_empty(NULL);
    floors[0]=active=capture();running=true;
    inherited_worker(9);worker_job=NULL;
    cave_vertical_capture(&state);
    for(int i=0;i<state.basin_count;i++)if(state.basins[i].depth==9) {
        state.basins[i].volume=0;state.basins[i].flags|=CAVE_VERTICAL_RESERVOIR|CAVE_VERTICAL_CHANGED;
    }
    assert(cave_vertical_restore(&state));level_image *lower=capture();floors[1]=lower;
    level_image_restore(active);cave_vertical_process_image(lower);
    assert_authored_water_empty(lower);
    level_image_restore(lower);
    for(int i=0;i<lower->vertical.portal_count;i++) {
        const cave_vertical_portal *p=&lower->vertical.portals[i];
        if(p->kind!=CAVE_VERTICAL_STREAM||p->peer_depth!=10)continue;
        assert(cave_feat[p->y][p->x]==FEAT_FLOOR);
        int direction=0;assert(cave_vertical_current(p->y,p->x,&direction)==0);
    }
    level_image_restore(active);level_cache_clear();
    puts("Real authored basin drain: full 15x7 tanks and distant outlet race empty on active and parked maps, empty outlet carries no current PASS.");
}

static int owned_volume(const level_image *image,u32b identity)
{
    for(int i=0;i<image->vertical.basin_count;i++) {
        const cave_vertical_basin *b=&image->vertical.basins[i];
        if(b->identity==identity&&b->depth==image->depth)return b->volume;
    }
    assert(false);return -1;
}

static void test_sluice_network(void)
{
    build_upper_state(false);u32b identity=blueprint().placements[0].identity;
    level_image *upper=capture();floors[0]=active=upper;running=true;
    inherited_worker(9);worker_job=NULL;
    level_image *lower=capture();floors[1]=lower;
    level_image_restore(upper);
    vertical_vault_marker gate;bool found=false;
    for(int i=0;vertical_vault_marker_at(i,&gate);i++)
        if(gate.role==VERTICAL_VAULT_GATE){found=true;break;}
    assert(found);p_ptr->py=gate.y;p_ptr->px=gate.x-1;
    cave_info[gate.y][gate.x]|=CAVE_SEEN;
    assert(cave_vertical_interact(gate.y,gate.x));
    level_image_capture(upper);
    /* Observe from the lower floor with exactly its two resident neighbors.
     * The upper sluice and lower race now exchange real cache packets. */
    fresh(10);cave_vertical_generation_begin(false);
    cave_vertical_generation_inherit(&lower->vertical,9,10);
    assert(cave_vertical_generation_finish());
    level_image *downstream=capture();floors[2]=downstream;
    level_image_restore(lower);active=lower;
    const int bank_y=27,bank_x=28; /* transformed lower row2,col19 */
    assert(cave_feat[bank_y][bank_x]==FEAT_FLOOR);
    int initial=owned_volume(upper,identity)+owned_volume(lower,identity)+owned_volume(downstream,identity);
    bool flooded=false;int peak=owned_volume(lower,identity);
    for(int tick=0;tick<80;tick++) {
        turn+=10;cave_vertical_process();
        int total=owned_volume(upper,identity)+owned_volume(lower,identity)+owned_volume(downstream,identity);
        for(int f=0;f<3;f++)for(int p=0;p<floors[f]->vertical.portal_count;p++) {
            const cave_vertical_portal *portal=&floors[f]->vertical.portals[p];
            if(portal->identity==identity&&(portal->flags&CAVE_VERTICAL_DOWN))total+=portal->pending;
        }
        assert(total==initial);
        peak=MAX(peak,owned_volume(lower,identity));
        flooded|=cave_feat[bank_y][bank_x]==FEAT_WATER;
    }
    assert(owned_volume(upper,identity)==0&&owned_volume(lower,identity)==72);
    assert(peak>96&&flooded&&cave_feat[bank_y][bank_x]==FEAT_FLOOR);
    level_image_restore(upper);active=upper;prune_cache();cave_vertical_refresh();
    assert(vertical_vault_get()->placements[0].reward_layers&1);
    int count=o_cnt;cave_vertical_refresh();assert(o_cnt==count);
    level_cache_clear();
    puts("Production three-floor sluice network: real Open drains upper cistern, temporarily floods then drains lower bank, exposes upper reward once, and conserves every unit including delayed packets PASS.");
}
'''


def main(case="all"):
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    harness = prefix + fixture_function("terminal_extra") + "\n" + fixture_function("reset_map")
    harness += TESTS + "\n" + init
    harness += "    assert(!init_quest_info());assert(!init_st_info());assert(!init_partition_info());\n"
    harness += "    style_info=(style_type*)style_head.info_ptr;style_name=style_head.name_ptr;\n"
    harness += "    assert(!init_flavor_info());assert(!init_effect_info());assert(!init_skeleton_note_info());\n"
    harness += "    assert(!init_h_info());assert(!init_n_info());build_randart_tables();flavor_init();drop_system_init();character_generated=true;\n"
    calls = {
        "all": "test_natural_passage_generation();test_cached_stair_and_long_fall_links();test_inherited_hole_connectivity();test_generation_upkeep_deferral();test_neighbor_reconstruction();test_regions_save();test_v5_metadata();test_public_cache_save();test_public_v5_cache();test_remote_transactions();test_aperture_visibility();test_packet_and_mirrors();test_shared_pair_history();test_cold_tube_reconstruction();test_authored_pool_drain();test_sluice_network();test_seeded_generation();",
        "upkeep": "test_generation_upkeep_deferral();",
        "holes": "test_inherited_hole_connectivity();",
        "links": "test_cached_stair_and_long_fall_links();",
        "natural": "test_natural_passage_generation();",
        "inherit": "test_neighbor_reconstruction();",
        "save": "test_regions_save();test_v5_metadata();test_public_cache_save();test_public_v5_cache();",
        "migration": "test_v5_metadata();test_public_v5_cache();",
        "public": "test_public_cache_save();",
        "generation": "test_seeded_generation();",
        "transactions": "test_remote_transactions();",
        "aperture": "test_aperture_visibility();",
        "packets": "test_packet_and_mirrors();",
        "baseline": "test_generation_baseline();",
        "shared": "test_shared_pair_history();",
        "tube": "test_cold_tube_reconstruction();",
        "pool": "test_authored_pool_drain();",
        "sluice": "test_sluice_network();",
    }
    harness += "    " + calls[case] + "\n"
    harness += '    puts("Vertical cache integration: PASS.");SDL_Quit();return 0;\n}\n'
    sources = []
    save = subprocess.check_output(["git", "show", "658f9417:src/fs/save.c"],
                                   cwd=ROOT, text=True, encoding="utf-8")
    start = save.index("void wr_monster(")
    legacy_monster = save[start:save.index("\n}", start) + 2]
    legacy_monster = legacy_monster.replace("void wr_monster(", "void fixture_legacy_monster(")
    for name, content in (("check.c", harness), ("writer.c", WRITER + LEGACY_WRITER + legacy_monster), ("reader.c", READER)):
        source = OUT / name
        source.write_text(content, encoding="utf-8")
        sources.append(str(source))
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/fs/save.c.obj", "/src/fs/load.c.obj", "/src/level-generation/level-cache.c.obj")
    objects = [p for p in objects if not p.endswith(excluded)]
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")), "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env.get("PATH", "")])
    exe = OUT / f"check-{case}.exe"
    wraps = ["level_cache_generation_links", "cave_vertical_generation_finish", "check_connectivity", "place_rubble_player", "terrain_history_finish", "update_view", "update_lore", "metarun_check_and_update_quests", "save_wr_monster"]
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g", "@CMakeFiles/sil-more.dir/includes_C.rsp", *sources, "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp", *("-Wl,--wrap=" + name for name in wraps), "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="data-", dir=OUT) as data:
        assert Path(data).resolve().is_relative_to(OUT.resolve())
        subprocess.run([str(exe), str(ROOT / "lib/edit"), data], cwd=data, env=env, check=True, timeout=180)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case", choices=("all", "links", "holes", "upkeep", "natural", "inherit", "save", "migration", "public", "generation", "transactions", "aperture", "packets", "baseline", "shared", "tube", "pool", "sluice"), default="all")
    main(parser.parse_args().case)

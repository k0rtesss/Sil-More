#!/usr/bin/env python3
"""Run the linked production dungeon pipeline with isolated generated data.

Requires build-incremental.ps1. Initializes production template/allocation
tables directly (no welcome menu, account state, config, or saved characters),
then invokes generate_cave, including retries, population and connectivity.
"""
import argparse
import os
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/dungeon-terrain-pipeline"

HARNESS = r'''
#include "angband.h"
#include "init/init2-internal.h"
#include "level-generation/level-generation-internal.h"
#include "level-generation/level-generation-terrain.h"
#include "level-generation/level-generation-terrain-access.h"
#include "level-generation/level-generation-landmarks.h"
#include "level-generation/level-generation-terrain-vaults.h"
#include "level-generation/level-generation-terrain-history.h"
#include "cave/cave-bridge.h"
#include "cave/cave-fixtures.h"
#include "log/log.h"
#include <assert.h>
#include <stdio.h>

const terrain_history_profile* __real_terrain_history_for_depth(int depth);
const terrain_history_profile* __wrap_terrain_history_for_depth(int depth) {
    static terrain_history_profile forced;
    const terrain_history_profile* original=__real_terrain_history_for_depth(depth);
    const char* history=getenv("SIL_TERRAIN_HISTORY_FIXTURE");
    const char* systems=getenv("SIL_TERRAIN_SYSTEM_COUNT");
    if(!history&&!systems)return original;
    forced=*original;
    if(history) {
        forced.ancient=!strcmp(history,"ancient")?100:0;
        forced.disaster=!strcmp(history,"disaster")?100:0;
        forced.overflow=!strcmp(history,"overflow")?100:0;
    }
    if(systems)forced.second_system_chance=atoi(systems)==2?100:0;
    return &forced;
}
static char config_path[1024];
static int generation_attempts;
static int generation_stage;
static char generation_detail[256];
void __real_level_gen_screen_set_stage(level_gen_screen_stage_t stage,cptr detail);
void __wrap_level_gen_screen_set_stage(level_gen_screen_stage_t stage,cptr detail) {
    generation_stage=stage;
    SDL_strlcpy(generation_detail,detail?detail:"",sizeof(generation_detail));
    __real_level_gen_screen_set_stage(stage,detail);
}
void __real_level_gen_screen_note_failure(cptr reason);
void __wrap_level_gen_screen_note_failure(cptr reason) {
    printf("RETRY attempt=%d stage=%d detail=%s reason=%s\n",generation_attempts,
        generation_stage,generation_detail,reason&&*reason?reason:"unspecified by headless production screen");
    __real_level_gen_screen_note_failure(reason);
}
void __real_level_gen_screen_start_attempt(void);
void __wrap_level_gen_screen_start_attempt(void) {
    generation_attempts++;
    assert(generation_attempts<=40);
    __real_level_gen_screen_start_attempt();
}
static byte terrain_before[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte terrain_after[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte critical_before[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte terrain_changed[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte system_before[TERRAIN_HISTORY_SYSTEMS][MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte system_after[TERRAIN_HISTORY_SYSTEMS][MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte system_critical[TERRAIN_HISTORY_SYSTEMS][MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static int original_components[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static int relocated_components[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte component_has_original_floor[MAX_DUNGEON_HGT*MAX_DUNGEON_WID+1];
extern int terrain_generation_components(const byte features[MAX_DUNGEON_HGT][MAX_DUNGEON_WID],
    int components[MAX_DUNGEON_HGT][MAX_DUNGEON_WID]);
static void assert_relocation(int old_y,int old_x,int y,int x) {
    assert(level_partition_index_for_point(old_y,old_x)==level_partition_index_for_point(y,x));
    int vault=terrain_vault_instance_at(old_y,old_x), destination=terrain_vault_instance_at(y,x);
    if(vault>=0 && vault!=destination) {
        assert(terrain_vault_policy_at(old_y,old_x));
        assert(destination<0 && ABS(y-old_y)+ABS(x-old_x)<=12);
        assert(cave_feat[y][x]==FEAT_FLOOR||cave_feat_is_bridge(cave_feat[y][x]));
    }
    if(old_y!=y || old_x!=x) {
        int origin=original_components[old_y][old_x],target=original_components[y][x];
        if(origin)assert(origin==target);
        else {
            /* Authored walls/rubble may contain an object before any walkable
             * component exists there. Its new bank must join original access. */
            assert(!terrain_generation_walkable(old_y,old_x,terrain_before));
            int final_component=relocated_components[y][x];
            assert(final_component>0&&component_has_original_floor[final_component]);
        }
    }
}
bool __real_place_terrain_landmark(void);
bool __real_terrain_landmark_realize(int system,int epoch);
static bool checked_terrain_operation(int system,int epoch) {
    int original_mon_max=mon_max,original_o_max=o_max;
    monster_type* monster_snapshot=malloc(sizeof(*mon_list)*mon_max);
    object_type* object_snapshot=malloc(sizeof(*o_list)*o_max);
    assert(monster_snapshot&&object_snapshot);
    memcpy(monster_snapshot,mon_list,sizeof(*mon_list)*mon_max);
    memcpy(object_snapshot,o_list,sizeof(*o_list)*o_max);
    memcpy(terrain_before,cave_feat,sizeof(terrain_before));
    terrain_generation_components((const byte (*)[MAX_DUNGEON_WID])cave_feat,original_components);
    memset(critical_before,0,sizeof(critical_before));
    for(int y=1;y<p_ptr->cur_map_hgt-1;y++) for(int x=1;x<p_ptr->cur_map_wid-1;x++) {
        int feat=cave_feat[y][x];
        int oi=cave_o_idx[y][x],mi=cave_m_idx[y][x];
        bool critical_object=false;
        for(int links=0;oi;oi=o_list[oi].next_o_idx) {
            assert(oi>0 && oi<o_max && ++links<o_max);
            if(o_list[oi].name1 || o_list[oi].tval==TV_NOTE)critical_object=true;
        }
        critical_before[y][x]=feat==FEAT_WALL_PERM || feat==FEAT_GLYPH
            || (feat>=FEAT_LESS && feat<=FEAT_MORE_SHAFT)
            || (feat>=FEAT_FORGE_HEAD && feat<=FEAT_FORGE_TAIL)
            || critical_object
            || (mi>0 && (r_info[mon_list[mi].r_idx].flags1&RF1_UNIQUE))
            || (cave_info[y][x]&(CAVE_MORGOTH_TUNNEL|CAVE_G_VAULT))
            || (terrain_vault_id_at(y,x)>=0&&!terrain_vault_policy_at(y,x));
    }
    bool accepted=system<0?__real_place_terrain_landmark():__real_terrain_landmark_realize(system,epoch);
    terrain_generation_components((const byte (*)[MAX_DUNGEON_WID])cave_feat,relocated_components);
    memset(component_has_original_floor,0,sizeof(component_has_original_floor));
    for(int y=1;y<p_ptr->cur_map_hgt-1;y++)for(int x=1;x<p_ptr->cur_map_wid-1;x++)
        if(original_components[y][x]&&relocated_components[y][x])
            component_has_original_floor[relocated_components[y][x]]=1;
    assert(mon_max==original_mon_max && o_max==original_o_max);
    for(int i=1;i<mon_max;i++) {
        if(!monster_snapshot[i].r_idx)continue;
        int old_y=monster_snapshot[i].fy,old_x=monster_snapshot[i].fx;
        assert_relocation(old_y,old_x,mon_list[i].fy,mon_list[i].fx);
        if(r_info[monster_snapshot[i].r_idx].flags1&RF1_UNIQUE)
            assert(old_y==mon_list[i].fy&&old_x==mon_list[i].fx);
        monster_snapshot[i].fy=mon_list[i].fy;monster_snapshot[i].fx=mon_list[i].fx;
        assert(!memcmp(&monster_snapshot[i],&mon_list[i],sizeof(monster_type)));
        assert(cave_m_idx[mon_list[i].fy][mon_list[i].fx]==i);
    }
    for(int i=1;i<o_max;i++) {
        if(!object_snapshot[i].k_idx)continue;
        if(!object_snapshot[i].held_m_idx) {
            int old_y=object_snapshot[i].iy,old_x=object_snapshot[i].ix;
            assert_relocation(old_y,old_x,o_list[i].iy,o_list[i].ix);
            if(object_snapshot[i].name1||object_snapshot[i].tval==TV_NOTE)
                assert(old_y==o_list[i].iy&&old_x==o_list[i].ix);
        }
        object_snapshot[i].iy=o_list[i].iy;object_snapshot[i].ix=o_list[i].ix;
        object_snapshot[i].next_o_idx=o_list[i].next_o_idx;
        assert(!memcmp(&object_snapshot[i],&o_list[i],sizeof(object_type)));
        if(!o_list[i].held_m_idx) {
            int oi=cave_o_idx[o_list[i].iy][o_list[i].ix],links=0;
            while(oi && oi!=i) {assert(++links<o_max);oi=o_list[oi].next_o_idx;}
            assert(oi==i);
        }
    }
    free(monster_snapshot);free(object_snapshot);
    memcpy(terrain_after,cave_feat,sizeof(terrain_after));
    if(system>=0) {
        memcpy(system_before[system],terrain_before,sizeof(terrain_before));
        memcpy(system_after[system],terrain_after,sizeof(terrain_after));
        memcpy(system_critical[system],critical_before,sizeof(critical_before));
    }
    return accepted;
}
bool __wrap_place_terrain_landmark(void) {return checked_terrain_operation(-1,-1);}
bool __wrap_terrain_landmark_realize(int system,int epoch) {return checked_terrain_operation(system,epoch);}
void __real_place_dungeon_terrain(void);
void __wrap_place_dungeon_terrain(void) {
    __real_place_dungeon_terrain();
    memcpy(terrain_after,cave_feat,sizeof(terrain_after));
    for(int y=0;y<MAX_DUNGEON_HGT;y++) for(int x=0;x<MAX_DUNGEON_WID;x++)
        terrain_changed[y][x]=terrain_before[y][x]!=cave_feat[y][x];
}
cptr __wrap_get_sdl_config_path(void) { return config_path; }
static errr dummy_xtra(int n, int v) { (void)n; (void)v; return 0; }
static term test_term;
static bool render_tiles;
static bool fixture_mode;
static void set_paths(const char* root, const char* out) {
    char path[1024];
    #define ASSET(field, name) do { snprintf(path,sizeof(path),"%s/lib/%s",root,name); field=SDL_strdup(path); } while(0)
    ASSET(ANGBAND_DIR, ""); ASSET(ANGBAND_DIR_EDIT, "edit");
    ASSET(ANGBAND_DIR_FILE, "file"); ASSET(ANGBAND_DIR_HELP, "help");
    ASSET(ANGBAND_DIR_INFO, "info"); ASSET(ANGBAND_DIR_PREF, "pref");
    ASSET(ANGBAND_DIR_XTRA, "xtra"); ASSET(ANGBAND_DIR_SCRIPT, "script");
    #undef ASSET
    #define PRIVATE(field, name) do { snprintf(path,sizeof(path),"%s/%s",out,name); assert(SDL_CreateDirectory(path)); field=SDL_strdup(path); } while(0)
    PRIVATE(ANGBAND_DIR_DATA,"data"); PRIVATE(ANGBAND_DIR_APEX,"meta");
    PRIVATE(ANGBAND_DIR_METARUN,"meta/metaruns"); PRIVATE(ANGBAND_DIR_SAVE,"save");
    PRIVATE(ANGBAND_DIR_USER,"user"); PRIVATE(ANGBAND_DIR_BONE,"bone");
    #undef PRIVATE
    snprintf(config_path,sizeof(config_path),"%s/user/sil_sdl.json",out);
}
static void initialize(void) {
    #define INIT(fn) do { printf("init %s\n",#fn); assert(fn()==0); } while(0)
    INIT(init_z_info); INIT(init_rt_info); INIT(init_f_info); INIT(init_k_info);
    INIT(init_b_info); INIT(init_a_info); ensure_artifact_guids(); ensure_artifact_spawn_numbers();
    INIT(init_e_info); INIT(init_r_info); INIT(init_v_info); INIT(init_h_info);
    INIT(init_st_info); INIT(init_style_info);
    style_info=(style_type*)style_head.info_ptr; style_name=style_head.name_ptr;
    INIT(init_partition_info); assert(terrain_themes_load());
    INIT(init_cu_info); INIT(init_mb_info); INIT(init_p_info); INIT(init_c_info);
    INIT(init_flavor_info); INIT(init_effect_info); INIT(init_skeleton_note_info);
    INIT(init_quest_info); INIT(init_oath_info); INIT(init_other); INIT(init_alloc);
    INIT(init_n_info); build_randart_tables();
    #undef INIT
    player_wipe(); /* Production birth resets monster limits and quest state. */
    rp_ptr=&p_info[0]; current_character_profile=&c_info[0];
    p_ptr->playing=true; p_ptr->chp=p_ptr->mhp=100;
    p_ptr->cur_map_hgt=p_ptr->cur_map_wid=66;
    playerturn=1; character_generated=true;
    flavor_init(); drop_system_init();
}
void export_tiles(const char*,int,int);
static void export_landmark_record(FILE* f,const terrain_landmark_stats* lm) {
    fprintf(f,",\"landmark\":{\"attempted\":%d,\"accepted\":%d,\"material\":%d,\"kind\":%d,\"tiles\":%d,\"span\":%d,\"partitions\":%d,\"bridges\":%d,\"lakes\":%d,\"tributaries\":%d,\"vaults\":%d,\"excavated\":%d,\"original_floor\":%d,\"relocated_monsters\":%d,\"relocated_objects\":%d,\"rejected_route\":%d,\"rejected_access\":%d,\"rejected_contents\":%d}",
        lm->attempted,lm->accepted,lm->material,lm->kind,lm->tiles,lm->span,lm->partitions,
        lm->bridges,lm->lakes,lm->tributaries,lm->vaults,lm->excavated,lm->original_floor,
        lm->relocated_monsters,lm->relocated_objects,lm->rejected_route,lm->rejected_access,lm->rejected_contents);
    fprintf(f,",\"network\":{\"family\":%d,\"basin_count\":%d,\"basin_tiles\":%d,\"channel_cells\":%d,\"narrow_cells\":%d,\"confluences\":%d,\"road_crossings\":%d,\"bridge_savings\":%d,\"min_span_percent\":%d}",
        lm->family,lm->basin_count,lm->basin_tiles,lm->channel_cells,lm->narrow_cells,
        lm->confluences,lm->road_crossings,lm->bridge_savings,
        terrain_landmark_for_depth(p_ptr->depth)->min_span_percent);
    fprintf(f,",\"flow\":{\"scenario\":%d,\"scenario_retries\":%d,\"sources\":%d,\"outlets\":%d,\"edge_mouths\":%d,\"springs\":%d,\"sinks\":%d,\"terminal_basins\":%d,\"vents\":%d,\"fissure_tips\":%d}",
        lm->scenario,lm->scenario_retries,lm->sources,lm->outlets,lm->edge_mouths,
        lm->springs,lm->sinks,lm->terminal_basins,lm->vents,lm->fissure_tips);
    fprintf(f,",\"structures\":{\"flooded\":%d,\"breached\":%d,\"repaired\":%d,\"rubble_tiles\":%d,\"repair_tiles\":%d,\"overflow_tiles\":%d}",
        lm->flooded_structures,lm->breached_structures,lm->repaired_structures,lm->rubble_tiles,lm->repair_tiles,lm->overflow_tiles);
}
static void export_landmark_grids(FILE* f,int system) {
    const char* keys[]={"features","partition_ids","new_terrain","reserved","info","natural",
        "terrain_before","terrain_after","critical_before","landmark_cells","landmark_bridges",
        "landmark_basins","landmark_channels","landmark_banks","landmark_terminals",
        "landmark_structures","landmark_rubble","landmark_repairs","vault_instances","vault_policies","landmark_roles","landmark_caps","initial_terrain"};
    for(int kind=0;kind<23;kind++) {
        if(system>=0&&(kind<6||kind==18||kind==19))continue;
        fprintf(f,",\"%s\":[",keys[kind]);
        for(int y=0;y<p_ptr->cur_map_hgt;y++) {
            fprintf(f,"%s[",y?",":"");
            for(int x=0;x<p_ptr->cur_map_wid;x++) {
                int work=system<0?terrain_landmark_cell(y,x):terrain_landmark_system_cell(system,y,x,0);
                bool hydraulic=work==TERRAIN_LANDMARK_TERRAIN||work==TERRAIN_LANDMARK_BRIDGE;
                int basin=system<0?terrain_landmark_basin_cell(y,x):terrain_landmark_system_cell(system,y,x,1);
                int channel=system<0?terrain_landmark_channel_cell(y,x):terrain_landmark_system_cell(system,y,x,2);
                int terminal=system<0?terrain_landmark_terminal_cell(y,x):terrain_landmark_system_cell(system,y,x,3);
                int structure=system<0?terrain_landmark_structure_cell(y,x):terrain_landmark_system_cell(system,y,x,4);
                int value=kind==0?cave_feat[y][x]:kind==1?level_partition_index_for_point(y,x):
                    kind==2?terrain_changed[y][x]:kind==3?terrain_generation_reserved(y,x):
                    kind==4?cave_info[y][x]:kind==5?cave_natural[y][x]:
                    kind==6?(system<0?terrain_before[y][x]:system_before[system][y][x]):
                    kind==7?(system<0?terrain_after[y][x]:system_after[system][y][x]):
                    kind==8?(system<0?critical_before[y][x]:system_critical[system][y][x]):
                    kind==9?hydraulic:kind==10?work==TERRAIN_LANDMARK_BRIDGE:
                    kind==11?(hydraulic?basin:0):kind==12?(hydraulic&&!basin&&channel):
                    kind==13?work==TERRAIN_LANDMARK_BANK:kind==14?terminal:kind==15?structure:
                    kind==16?work==TERRAIN_LANDMARK_RUBBLE:kind==17?work==TERRAIN_LANDMARK_REPAIR:
                    kind==18?terrain_vault_instance_at(y,x):kind==19?terrain_vault_policy_at(y,x):
                    system<0?0:terrain_landmark_system_cell(system,y,x,kind-15);
                fprintf(f,"%s%d",x?",":"",value);
            }
            fprintf(f,"]");
        }
        fprintf(f,"]");
    }
}
static void export_map(const char* out,int depth,int seed,Uint64 elapsed) {
    char path[1024]; snprintf(path,sizeof(path),"%s/depth-%d-seed-%d.json",out,depth,seed);
    FILE* f=fopen(path,"w"); assert(f);
    const terrain_generation_stats* s=terrain_generation_last_stats();
    fprintf(f,"{\"depth\":%d,\"seed\":%d,\"height\":%d,\"width\":%d,\"attempts\":%d,\"elapsed_ms\":%llu,\"rooms\":%d,\"monsters\":%d,\"objects\":%d,\"player\":[%d,%d],\"accepted\":%d,\"tiles\":%d,\"proposals\":%d,\"bridges\":%d,\"partitions\":[",
        depth,seed,p_ptr->cur_map_hgt,p_ptr->cur_map_wid,generation_attempts,
        (unsigned long long)elapsed,dun->cent_n,mon_max-1,o_max-1,p_ptr->py,p_ptr->px,
        s->accepted,s->tiles,s->proposals,s->bridges);
    for(int pi=0;pi<current_partition_count;pi++)
        fprintf(f,"%s{\"id\":%d,\"mode\":%d,\"big_cave_type\":%d}",pi?",":"",pi,current_partition_modes[pi],current_partition_big_cave_types[pi]);
    fprintf(f,"],\"material_features\":[");
    for(int i=0;i<TERRAIN_THEME_MATERIAL_MAX;i++) fprintf(f,"%s%d",i?",":"",s->material_features[i]);
    fprintf(f,"],\"material_tiles\":[");
    for(int i=0;i<TERRAIN_THEME_MATERIAL_MAX;i++) fprintf(f,"%s%d",i?",":"",s->material_tiles[i]);
    fprintf(f,"]");
    fprintf(f,",\"fixture\":%s,\"generation_depth\":%d",fixture_mode?"true":"false",p_ptr->depth);
    export_landmark_record(f,terrain_landmark_last_stats());
    export_landmark_grids(f,-1);
    fprintf(f,",\"systems\":[");
    for(int system=0;system<terrain_history_count();system++) {
        fprintf(f,"%s{\"system\":%d,\"epoch\":%d",system?",":"",system,terrain_history_epoch(system));
        export_landmark_record(f,terrain_landmark_system_stats(system));
        export_landmark_grids(f,system);fprintf(f,"}");
    }
    fprintf(f,"]");
    fprintf(f,"}\n"); fclose(f);
    if(render_tiles)export_tiles(out,depth,seed);
}
#include "../../tests/terrain-landmark-test.c"
int main(int argc, char** argv) {
    assert(argc==5||argc==6); setbuf(stdout,NULL); log_set_level(LOG_WARN);
    render_tiles=getenv("SIL_TERRAIN_PREVIEW")!=NULL;
    assert(SDL_Init(SDL_INIT_EVENTS|(render_tiles?SDL_INIT_VIDEO:0)));
    set_paths(argv[1],argv[2]);
    term_init(&test_term,80,32,256); test_term.xtra_hook=dummy_xtra;
    Term_activate(&test_term); term_screen=&test_term; angband_term[0]=&test_term;
    initialize();
    if(argc==6 && !strcmp(argv[5],"fixtures")) {
        fixture_mode=true;
        run_landmark_fixtures(argv[2]);return 0;
    }
    int depth=atoi(argv[3]), seed=atoi(argv[4]);
    p_ptr->depth=p_ptr->max_depth=depth;
    p_ptr->create_stair=FEAT_LESS;
    Rand_state_init((u64b)seed);
    printf("GENERATE depth=%d seed=%d\n",depth,seed);
    Uint64 started=SDL_GetTicks();
    generate_cave();
    Uint64 elapsed=SDL_GetTicks()-started;
    assert(character_dungeon && in_bounds_fully(p_ptr->py,p_ptr->px));
    assert(cave_m_idx[p_ptr->py][p_ptr->px]==-1);
    assert(mon_max>1 && o_max>1);
    const terrain_generation_stats* stats=terrain_generation_last_stats();
    int stairs=0, terrain=0;
    for(int y=1;y<p_ptr->cur_map_hgt-1;y++) for(int x=1;x<p_ptr->cur_map_wid-1;x++) {
        int feat=cave_feat[y][x];
        assert(feat>FEAT_NONE && feat<z_info->f_max);
        if(feat==FEAT_MORE||feat==FEAT_MORE_SHAFT) stairs++;
        if(feat==FEAT_CHASM||feat==FEAT_LAVA||feat==FEAT_POISON||feat==FEAT_WATER||feat==FEAT_ICE) terrain++;
    }
    if(depth<MORGOTH_DEPTH) assert(stairs>0);
    const char* forced_history=getenv("SIL_TERRAIN_HISTORY_FIXTURE");
    const char* forced_systems=getenv("SIL_TERRAIN_SYSTEM_COUNT");
    if(forced_systems)assert(terrain_history_count()==atoi(forced_systems));
    if(forced_history) {
        int expected=!strcmp(forced_history,"ancient")?0:!strcmp(forced_history,"disaster")?1:2;
        int overflow_added=0;
        for(int system=0;system<terrain_history_count();system++) {
            assert(terrain_history_epoch(system)==expected);
            overflow_added+=terrain_landmark_system_stats(system)->overflow_tiles;
        }
        if(expected==2)assert(overflow_added>0);
    }
    export_map(argv[2],depth,seed,elapsed);
    const terrain_landmark_stats* lm=terrain_landmark_last_stats();
    printf("PIPELINE PASS depth=%d seed=%d size=%dx%d rooms=%d monsters=%d objects=%d down=%d terrain=%d features=%d tiles=%d proposals=%d attempts=%d elapsed_ms=%llu landmark=%d landmark_tiles=%d span=%d partitions=%d bridges=%d landmark_attempts=%d\n",
        depth,seed,p_ptr->cur_map_hgt,p_ptr->cur_map_wid,dun->cent_n,mon_max-1,o_max-1,stairs,terrain,
        stats->accepted,stats->tiles,stats->proposals,generation_attempts,(unsigned long long)elapsed,
        lm->accepted,lm->tiles,lm->span,lm->partitions,lm->bridges,lm->attempted);
    return 0;
}
'''


RENDERER = r'''
#include "angband.h"
#include "sdl/main-sdl-private.h"
#include "level-generation/level-generation-landmarks.h"
#include "level-generation/level-generation-terrain-history.h"
#include <assert.h>
static int preview_system=-1;
static int preview_cell(int y,int x) {return preview_system<0?terrain_landmark_cell(y,x):terrain_landmark_system_cell(preview_system,y,x,0);}
static int preview_basin(int y,int x) {return preview_system<0?terrain_landmark_basin_cell(y,x):terrain_landmark_system_cell(preview_system,y,x,1);}
static int preview_channel(int y,int x) {return !preview_basin(y,x)&&(preview_system<0?terrain_landmark_channel_cell(y,x):terrain_landmark_system_cell(preview_system,y,x,2));}
static int preview_terminal(int y,int x) {return preview_system<0?terrain_landmark_terminal_cell(y,x):terrain_landmark_system_cell(preview_system,y,x,3);}
static int preview_structure(int y,int x) {return preview_system<0?terrain_landmark_structure_cell(y,x):terrain_landmark_system_cell(preview_system,y,x,4);}
static const terrain_landmark_stats* preview_stats(void) {return preview_system<0?terrain_landmark_last_stats():terrain_landmark_system_stats(preview_system);}
/* Use the production layered renderer and shipped tiles, with the entire map
 * revealed for inspection. This is a static diagnostic view, not a playtest. */
static void export_one_system(const char* out,int depth,int seed) {
    char stem[128];
    if(preview_system<0)snprintf(stem,sizeof(stem),"depth-%d-seed-%d",depth,seed);
    else snprintf(stem,sizeof(stem),"depth-%d-seed-%d-system-%d",depth,seed,preview_system);
    if(!g_state.renderer) {
        sdl_config_set_defaults(&config);
        config.min_terminal_mode=false;
        assert(SDL_CreateWindowAndRenderer("Terrain map preview",1024,768,
            SDL_WINDOW_HIDDEN,&g_state.window,&g_state.renderer));
        g_state.use_tiles=true;assert(sdl_load_tileset_texture());
        use_graphics=GRAPHICS_MICROCHASM;
    }
    int cy=p_ptr->py,cx=p_ptr->px,best=-1000000;
    int basin_sizes[256]={0};
    const char* crop_reason="Player vicinity; no accepted network";
    for(int y=1;y<p_ptr->cur_map_hgt-1;y++)for(int x=1;x<p_ptr->cur_map_wid-1;x++) {
        cave_info[y][x]|=CAVE_MARK|CAVE_SEEN;
        cave_light[y][x]=MAX(2,cave_light[y][x]);
        int basin=preview_basin(y,x);
        if(basin>0&&basin<256)basin_sizes[basin]++;
    }
    /* Every candidate is an actual committed network cell. A map-center
     * preference must never select empty space between isolated pools. */
    for(int y=1;y<p_ptr->cur_map_hgt-1;y++)for(int x=1;x<p_ptr->cur_map_wid-1;x++) {
        int cell=preview_cell(y,x),basin=preview_basin(y,x);
        bool spine=preview_channel(y,x);
        if(cell!=TERRAIN_LANDMARK_TERRAIN&&cell!=TERRAIN_LANDMARK_BRIDGE)continue;
        int neighbors=preview_channel(y-1,x)+preview_channel(y+1,x)
            +preview_channel(y,x-1)+preview_channel(y,x+1);
        int score=0;const char* reason="Actual network terrain";
        if(preview_stats()->family==TERRAIN_LANDMARK_FRACTURE_FAMILY) {
            if(spine) {score=100000;reason="Actual fracture spine near its middle";}
            if(spine&&neighbors>=3) {score=400000;reason="Actual fracture branch junction";}
        } else if(basin>0&&basin<256) {
            int interior=0,nearby_channels=0;
            for(int radius=1;radius<=8;radius++) {
                bool enclosed=true;
                for(int dy=-radius;dy<=radius;dy++)for(int dx=-radius;dx<=radius;dx++)
                    if(preview_basin(y+dy,x+dx)!=basin)enclosed=false;
                if(!enclosed)break;
                interior=radius;
            }
            for(int dy=-8;dy<=8;dy++)for(int dx=-8;dx<=8;dx++)
                nearby_channels+=preview_channel(y+dy,x+dx);
            score=200000+basin_sizes[basin]*100+interior*100+MIN(nearby_channels,50);
            reason="Interior of a substantial basin";
            if(neighbors) {score+=200000;reason="Basin shore where a channel joins";}
        } else if(spine) {
            score=100000;reason="Actual channel spine near its middle";
            if(neighbors>=3) {score+=50000;reason="Actual channel branch junction";}
        }
        score-=ABS(y-p_ptr->cur_map_hgt/2)+ABS(x-p_ptr->cur_map_wid/2);
        if(score>best) {best=score;cy=y;cx=x;crop_reason=reason;}
    }
    if(preview_stats()->accepted)
        assert(preview_cell(cy,cx)==TERRAIN_LANDMARK_TERRAIN
            ||preview_cell(cy,cx)==TERRAIN_LANDMARK_BRIDGE);
    int terminal_y=-1,terminal_x=-1,terminal_score=-1;
    int structure_y=-1,structure_x=-1,structure_score=-1;
    for(int y=1;y<p_ptr->cur_map_hgt-1;y++)for(int x=1;x<p_ptr->cur_map_wid-1;x++) {
        int terminal=preview_terminal(y,x);
        int cell=preview_cell(y,x), effect=preview_structure(y,x);
        int score=terminal==TERRAIN_TERMINAL_VENT?500:terminal==TERRAIN_TERMINAL_SPRING?400:
            terminal==TERRAIN_TERMINAL_SINK?300:terminal==TERRAIN_TERMINAL_EDGE?200:100;
        if(terminal&&score>terminal_score) {terminal_y=y;terminal_x=x;terminal_score=score;}
        score=cell==TERRAIN_LANDMARK_REPAIR?500:cell==TERRAIN_LANDMARK_RUBBLE?400:
            cell==TERRAIN_LANDMARK_BRIDGE&&effect==3?450:effect==2?200:100;
        if(effect&&score>structure_score) {structure_y=y;structure_x=x;structure_score=score;}
    }
    char metadata_path[1024];snprintf(metadata_path,sizeof(metadata_path),
        "%s/preview-%s.json",out,stem);
    FILE* metadata=fopen(metadata_path,"w");assert(metadata);
    fprintf(metadata,"{\"center\":[%d,%d],\"feature\":%d,\"reason\":\"%s\",\"terminal_center\":[%d,%d],\"structure_center\":[%d,%d]}\n",
        cy,cx,cave_feat[cy][cx],crop_reason,terminal_y,terminal_x,structure_y,structure_x);fclose(metadata);
    for(int crop=0;crop<4;crop++) {
        if(preview_system>=0&&!crop)continue;
        if((crop==2&&terminal_y<0)||(crop==3&&structure_y<0))continue;
        int crop_y=crop==2?terminal_y:crop==3?structure_y:cy;
        int crop_x=crop==2?terminal_x:crop==3?structure_x:cx;
        bool quartz_probe=false;
        int h=crop?MIN(25,p_ptr->cur_map_hgt):p_ptr->cur_map_hgt;
        int w=crop?MIN(35,p_ptr->cur_map_wid):p_ptr->cur_map_wid;
        int y0=crop?MAX(0,MIN(crop_y-h/2,p_ptr->cur_map_hgt-h)):0;
        int x0=crop?MAX(0,MIN(crop_x-w/2,p_ptr->cur_map_wid-w)):0;
        int scale=crop?2:1;
        SDL_Texture* previous=SDL_GetRenderTarget(g_state.renderer);
        SDL_Texture* target=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
            SDL_TEXTUREACCESS_TARGET,w*16*scale,h*16*scale);assert(target);
        SDL_SetRenderTarget(g_state.renderer,target);
        SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255);SDL_RenderClear(g_state.renderer);
        for(int y=y0;y<y0+h;y++)for(int x=x0;x<x0+w;x++) {
            byte a,ta;char c,tc;map_info(y,x,&a,&c,&ta,&tc);
            if(crop&&!quartz_probe&&cave_feat[y][x]==FEAT_QUARTZ) {
                quartz_probe=true;
                printf("PREVIEW QUARTZ y=%d x=%d feat=%d style_color=%d display=%d:%d underlay=%d:%d\n",
                    y,x,cave_feat[y][x],cave_color[y][x],
                    a&TILE_INDEX_MASK,(byte)c&TILE_INDEX_MASK,
                    ta&TILE_INDEX_MASK,(byte)tc&TILE_INDEX_MASK);
            }
            SDL_FRect dst={(x-x0)*16*scale,(y-y0)*16*scale,16*scale,16*scale};
            sdl_draw_map_tile_layers_at(y,x,a,c,ta,tc,&dst);
        }
        SDL_Surface* surface=SDL_RenderReadPixels(g_state.renderer,NULL);assert(surface);
        char path[1024];snprintf(path,sizeof(path),"%s/%s-%s.png",
            out,stem,crop==3?"structure-tiles":crop==2?"terminal-tiles":crop?"room-tiles":"game-tiles");
        assert(IMG_SavePNG(surface,path));SDL_DestroySurface(surface);
        SDL_SetRenderTarget(g_state.renderer,previous);SDL_DestroyTexture(target);
    }
}
void export_tiles(const char* out,int depth,int seed) {
    preview_system=-1;export_one_system(out,depth,seed);
    for(int system=0;system<terrain_history_count();system++) {
        preview_system=system;export_one_system(out,depth,seed);
    }
    preview_system=-1;
}

'''


def main():
    global OUT
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--depths", nargs="+", type=int, default=[3, 7, 10, 13, 16, 19, 20])
    parser.add_argument("--seeds", nargs="+", type=int, default=[1301, 7307, 9931])
    parser.add_argument("--timeout", type=int, default=90)
    parser.add_argument("--fixtures", action="store_true", help="Run deterministic linked landmark shape fixtures")
    parser.add_argument("--tiles", action="store_true", help="Render full-map and room crops through the production SDL renderer")
    parser.add_argument("--history",choices=("ancient","disaster","overflow"),help="Force a historical phase through an isolated profile wrapper")
    parser.add_argument("--systems",type=int,choices=(1,2),help="Force the requested system-count roll without editing themes")
    parser.add_argument("--output",type=Path,default=OUT,help="Isolated output directory for forced scenarios")
    args = parser.parse_args()
    OUT=args.output.resolve()
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS.replace("../../tests/terrain-landmark-test.c",str(ROOT/"scripts/tests/terrain-landmark-test.c").replace("\\","/")), encoding="utf-8")
    preview = OUT / "preview.c"
    preview.write_text(RENDERER, encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    rsp = OUT / "objects.rsp"
    rsp.write_text("\n".join('"' + p + '"' for p in objects if not p.endswith("/src/main.c.obj")), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(["C:/msys64/mingw64/bin", "C:/msys64/usr/bin",
        *[str(BUILD / "_deps" / x) for x in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")], env["PATH"]])
    env["SDL_VIDEO_DRIVER"] = "dummy"
    env["SDL_RENDER_DRIVER"] = "software"
    if args.tiles:
        env["SIL_TERRAIN_PREVIEW"] = "1"
    if args.history:env["SIL_TERRAIN_HISTORY_FIXTURE"]=args.history
    if args.systems:env["SIL_TERRAIN_SYSTEM_COUNT"]=str(args.systems)
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), str(preview), "@" + str(rsp),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-Wl,--wrap=get_sdl_config_path",
        "-Wl,--wrap=place_dungeon_terrain", "-Wl,--wrap=place_terrain_landmark",
        "-Wl,--wrap=terrain_landmark_realize",
        "-Wl,--wrap=terrain_theme_for_depth", "-Wl,--wrap=terrain_landmark_for_depth",
        "-Wl,--wrap=terrain_network_for_depth", "-Wl,--wrap=terrain_history_for_depth",
        "-Wl,--wrap=level_gen_screen_start_attempt",
        "-Wl,--wrap=level_gen_screen_set_stage", "-Wl,--wrap=level_gen_screen_note_failure", "-o", str(exe)],
        cwd=BUILD, env=env, check=True)
    if args.fixtures:
        fixture_out = OUT / "fixtures"
        fixture_out.mkdir(exist_ok=True)
        subprocess.run([str(exe),str(ROOT),str(fixture_out),"10","1","fixtures"],
            cwd=ROOT,env=env,check=True,timeout=args.timeout)
        return
    for depth in args.depths:
        for seed in args.seeds:
            log_path = OUT / f"depth-{depth}-seed-{seed}.log"
            with log_path.open("w", encoding="utf-8") as log:
                result = subprocess.run([str(exe), str(ROOT), str(OUT), str(depth), str(seed)],
                    cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=args.timeout)
            output = log_path.read_text(encoding="utf-8")
            if result.returncode:
                print(output[-6000:])
                raise RuntimeError(f"Pipeline failed ({result.returncode}): {log_path}")
            summary = next((line for line in output.splitlines() if line.startswith("PIPELINE PASS")), None)
            assert summary, f"Missing pipeline acceptance: {log_path}"
            print(summary, flush=True)


if __name__ == "__main__":
    main()

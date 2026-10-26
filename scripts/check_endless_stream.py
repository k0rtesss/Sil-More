#!/usr/bin/env python3
"""Production cache/site ownership and stream save migration fixture.

Uses real cache transactions, bounded construction, archive and serializers.
Only initial scenes are assembled; no player file or live game is opened.
"""
from pathlib import Path
import argparse
import os
import re
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function
from check_vertical_cache import WRITER, READER

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/endless-stream"

TESTS = r'''
#include "level-generation/level-stream.h"
#include "level-generation/level-state.h"
#include "level-generation/level-generation-terrain-history.h"
#include "level-generation/level-generation-terrain-vaults.h"
#include "cave/cave-fixtures.h"
#include "cave/cave-flood.h"
#include "cave/cave-water-flow.h"
#define write_regions cache_write_regions
#define read_regions cache_read_regions
#include "level-generation/level-cache.c"
#undef write_regions
#undef read_regions
#include "level-generation/level-stream-sites.c"
void fixture_write_begin(byte*,size_t);
size_t fixture_write_end(void);
void fixture_read_begin(const byte*,size_t,int);
size_t fixture_read_end(void);
void fixture_legacy_mode(bool);
size_t fixture_full_write(byte*,size_t);
errr fixture_full_read(const byte*,size_t);
static byte encoded[16*1024*1024];
static u64b survivor_token, killed_token, chest_token, moved_token;
static u64b unique_token;
static int resident_race;
static int marked_chest;
static void fixture_fresh(int depth, int size)
{
    character_dungeon=false;
    memset(p_ptr,0,sizeof(*p_ptr)); reset_map(depth);
    p_ptr->cur_map_hgt=p_ptr->cur_map_wid=size;
    p_ptr->py=p_ptr->px=size/2;
    p_ptr->chp=p_ptr->mhp=100; p_ptr->food=PY_FOOD_FULL;
    p_ptr->energy=87; p_ptr->max_depth=depth; p_ptr->playing=true;
    turn=playerturn=1234;
    for(int y=0;y<size;y++) for(int x=0;x<size;x++) {
        bool edge=!y||!x||y==size-1||x==size-1;
        cave_feat[y][x]=edge?FEAT_WALL_PERM:FEAT_FLOOR;
        cave_info[y][x]=edge?CAVE_WALL:0;
        cave_color[y][x]=COLOR_STYLE_BASE+1;
        cave_when[y][x]=cave_natural[y][x]=cave_rewired[y][x]=0;
    }
    remember_partition_grid(size==LEVEL_STREAM_SIZE?5:1,size==LEVEL_STREAM_SIZE?5:1,
        size==LEVEL_STREAM_SIZE?25:1);
    cave_environment_reset(); cave_atmosphere_reset(); cave_fixtures_clear();
    cave_flood_clear(); cave_water_flow_reset(); cave_vertical_reset(); vertical_vault_reset();
    terrain_generation_reset(); terrain_history_reset(); terrain_vault_reset();
    hint_messages_clear_for_load(depth,size,size); styles_init_for_level();
    for(int i=FLOW_WANDERING_HEAD;i<=FLOW_WANDERING_TAIL;i++)
        flow_center_y[i]=flow_center_x[i]=wandering_pause[i]=0;
    scent_when=100;
    level_stream_set_frame((level_stream_frame){.seed=817239,.serial=1,.y=-88,.x=-88,.enabled=true});
}
static level_image* fixture_capture(void)
{ level_image* im=level_image_new();assert(im);level_image_capture(im);im->visited=true;return im; }
static int kind_for(int tval)
{ for(int i=1;i<z_info->k_max;i++)if(k_info[i].tval==tval)return i;assert(false);return 0; }
static int object_at(int y,int x,int tval,int pval)
{
    object_type o;object_prep(&o,kind_for(tval));o.number=1;o.pval=pval;
    int n=floor_carry(y,x,&o);assert(n);return n;
}
static int token_mon(u64b token)
{ for(int i=1;i<mon_max;i++)if(mon_list[i].r_idx&&mon_list[i].site_token==token)return i;return 0; }
static int token_obj(u64b token)
{ for(int i=1;i<o_max;i++)if(o_list[i].k_idx&&o_list[i].site_token==token)return i;return 0; }
static void fixture_clear_live_lists(void)
{
    character_dungeon=false;
    mon_max=o_max=1;mon_cnt=o_cnt=num_repro=0;
    memset(mon_list,0,MAX_MONSTERS*sizeof(*mon_list));
    memset(o_list,0,z_info->o_max*sizeof(*o_list));
    memset(cave_m_idx,0,MAX_DUNGEON_HGT*sizeof(*cave_m_idx));
    memset(cave_o_idx,0,MAX_DUNGEON_HGT*sizeof(*cave_o_idx));
}
static void fixture_scene(void)
{
    level_cache_clear(); level_stream_clear();
    fixture_fresh(19,LEVEL_STREAM_SIZE);floors[1]=fixture_capture();
    fixture_fresh(20,LEVEL_STREAM_SIZE);
    for(int y=78;y<=102;y++)for(int x=78;x<=102;x++)
        cave_info[y][x]|=CAVE_ICKY|CAVE_G_VAULT|CAVE_ROOM|CAVE_MARK;
    cave_feat[82][82]=FEAT_DOOR_HEAD;
    cave_feat[83][83]=FEAT_WALL_INNER;
    marked_chest=object_at(90,90,TV_CHEST,7);
    int moved=object_at(94,94,TV_FOOD,514);
    o_list[moved].number=2; /* Later split into two real one-item carried stacks. */
    resident_race=0;
    for(int i=1;i<z_info->r_max;i++)if(r_info[i].name
        && !(r_info[i].flags1&RF1_UNIQUE) && !(r_info[i].flags2&RF2_MULTIPLY)
        && !(r_info[i].flags4&RF4_BRTH_FIRE) && r_info[i].level<=10) { resident_race=i;break; }
    assert(resident_race);
    assert(place_monster_one(88,88,resident_race,true,false,NULL));
    assert(place_monster_one(92,92,resident_race,true,false,NULL));
    current_build_vault_exact_token=true;
    assert(place_monster_one(95,95,403,true,true,NULL));
    current_build_vault_exact_token=false;
    int survivor=cave_m_idx[88][88],killed=cave_m_idx[92][92];
    mon_list[survivor].hp=mon_list[survivor].maxhp=17;
    mon_list[survivor].routine.home_y=88;mon_list[survivor].routine.home_x=88;
    mon_list[survivor].routine.style=2;mon_list[survivor].routine.territory=0;
    mon_list[survivor].routine.count=12;mon_list[survivor].routine.next=0;
    const byte patrol_y[]={87,87,87,87,88,89,90,90,90,90,89,88};
    const byte patrol_x[]={87,88,89,90,90,90,90,89,88,87,87,87};
    memcpy(mon_list[survivor].routine.y,patrol_y,12);memcpy(mon_list[survivor].routine.x,patrol_x,12);
    mon_list[survivor].world.observation_y=89;mon_list[survivor].world.observation_x=90;
    dun->cent_n=1;dun->cent[0]=(coord){90,90};
    dun->corner[0]=(rectangle){78,78,102,102};
    dun->kind[0]=ROOM_KIND_GREATER_VAULT;dun->is_quest[0]=false;
    SDL_strlcpy(g_vault_name,"Fixture permanent hall",sizeof(g_vault_name));
    level_stream_record_generation_sites();
    floors[0]=active=fixture_capture();running=true;requested_depth=pending_depth=-1;
    level_cache_stream_capture_sites();
    assert(site_count==1);
    survivor_token=mon_list[survivor].site_token;killed_token=mon_list[killed].site_token;
    chest_token=o_list[marked_chest].site_token;moved_token=o_list[moved].site_token;
    unique_token=mon_list[cave_m_idx[95][95]].site_token;
    assert(survivor_token&&killed_token&&chest_token&&moved_token);
    /* Real gameplay mutations follow initial ownership registration. */
    cave_set_feat(82,82,FEAT_OPEN);cave_set_feat(83,83,FEAT_DAMAGED_WALL);
    o_list[marked_chest].pval=0;o_list[marked_chest].ident|=IDENT_KNOWN;
    delete_monster_idx(killed);
    cave_info[110][150]|=CAVE_MARK;cave_feat[110][150]=FEAT_RUBBLE;
    object_at(112,152,TV_FOOD,913);
    char lines[1][100]={"Ordinary local clue must expire"};
    hint_message_meta meta={0};meta.source_y=112;meta.source_x=152;
    meta.destination_count=1;meta.destinations[0]=(hint_message_destination){.kind=HINT_DESTINATION_FIXED_FEATURE,.y=110,.x=150};
    assert(hint_messages_add_for_load(lines,1,&meta)>=0);
    environment_state env={.random=8123,.last_turn=turn,.source_count=2,.mineral_budget=6,.ready=true};
    assert(cave_environment_restore_state(env));
    environment_source source={110,160,FEAT_WATER,8,2,1,1434};
    cave_feat[110][160]=FEAT_WATER;
    cave_feat[110][161]=FEAT_WATER;
    assert(cave_environment_restore_source(0,source));
    environment_cell cell={.base_feat=FEAT_FLOOR,.known_feat=FEAT_WATER,.flags=ENV_ADDED_LIQUID,.integrity=100,.owner=1,.due=1334};
    assert(cave_environment_restore_cell(110,160,cell));
    assert(cave_environment_restore_cell(110,161,cell));
    environment_source hall_source={86,86,FEAT_WATER,7,2,1,1434};
    assert(cave_environment_restore_source(1,hall_source));
    cave_feat[86][86]=cave_feat[86][87]=FEAT_WATER;
    cell.owner=2;assert(cave_environment_restore_cell(86,86,cell));
    assert(cave_environment_restore_cell(86,87,cell));
    cave_event_emit(CAVE_EVENT_DIG,110,155,8);
    level_image_capture(active);
}
static void shift(int dy,int dx)
{
    p_ptr->py=p_ptr->px=110;
    level_stream_frame f=level_stream_get_frame();
    s32b t=turn,pt=playerturn;int food=p_ptr->food,hp=p_ptr->chp,energy=p_ptr->energy;
    u64b rng=Rand_state_export();
    assert(level_cache_rebase_window(dy,dx));
    level_stream_frame after=level_stream_get_frame();
    assert(after.y==f.y+dy&&after.x==f.x+dx);
    assert(turn==t&&playerturn==pt&&p_ptr->food==food&&p_ptr->chp==hp&&p_ptr->energy==energy);
    assert(Rand_state_export()==rng);
    assert(!job); /* Depth20+resident19: no asynchronous neighbour is missing. */
}
static void verify_site(bool check_observation)
{
    level_stream_frame f=level_stream_get_frame();
    assert(f.y==-88&&f.x==-88);
    assert(cave_feat[82][82]==FEAT_OPEN&&cave_feat[83][83]==FEAT_DAMAGED_WALL);
    int c=token_obj(chest_token),m=token_mon(survivor_token);
    assert(c&&o_list[c].pval==0&&(o_list[c].ident&IDENT_KNOWN));
    assert(m&&mon_list[m].hp==17&&mon_list[m].fy==88&&mon_list[m].fx==88);
    assert(mon_list[m].social_state==MON_SOCIAL_NONE&&!mon_list[m].social_rival
        &&!mon_list[m].social_focus&&!mon_list[m].social_ally);
    assert(mon_list[m].routine.home_y==88&&mon_list[m].routine.home_x==88);
    if(check_observation)
        assert(mon_list[m].world.observation_y==89&&mon_list[m].world.observation_x==90);
    assert(!token_mon(killed_token));
    assert(token_mon(unique_token));
    int copies=0;
    for(int i=1;i<o_max;i++)copies+=o_list[i].k_idx&&o_list[i].site_token==chest_token;
    assert(copies==1);
    bool reservoir=false;
    environment_state env=cave_environment_get_state();
    for(int i=0;i<env.source_count;i++) {
        const environment_source* source=cave_environment_source_at(i);
        if(source->y==86&&source->x==86) {
            assert(source->capacity==7&&source->used==2&&source->next_turn==1434+(turn-1234));
            reservoir=true;
        }
    }
    assert(reservoir);
    assert(cave_environment_cell_at(86,86)->due==1334+(turn-1234));
}
static void test_horizon(void)
{
    fixture_scene();
    u16b info=cave_info[110][150];
    shift(0,44);
    assert(cave_feat[110][106]==FEAT_RUBBLE&&cave_info[110][106]==(info&~(CAVE_VIEW|CAVE_SEEN|CAVE_TEMP)));
    assert(cave_environment_cell_at(110,116)->due==1334);
    const environment_source* s=cave_environment_source_at(0);
    assert(s&&s->x==116&&s->capacity==8&&s->used==2&&s->next_turn==1434);
    for(int i=1;i<6;i++)shift(0,44);
    assert(level_stream_get_frame().x==176);
    assert(!token_mon(survivor_token)&&!token_obj(chest_token));
    assert(site_count==1&&sites[0].actor_count==2);
    assert(hint_messages_count_for_save()==0);
    for(int i=0;i<6;i++)shift(0,-44);
    verify_site(true);
    assert(!(cave_info[110][150]&CAVE_MARK));
    for(int i=1;i<o_max;i++)assert(!o_list[i].k_idx||o_list[i].pval!=913);
    for(int i=0;i<6;i++)shift(-44,0);
    assert(level_stream_get_frame().y==-352);
    for(int i=0;i<6;i++)shift(44,0);
    verify_site(false); /* The observation exits one tile before its actor. */
    puts("PASS 24 real cache rebases, ordinary overlap/forgetting, archived hall mutations/death, clocks/RNG, negative world origins and source budget");
}
static void test_transfer(void)
{
    fixture_scene();
    int o=token_obj(moved_token);assert(o);
    assert(level_cache_transfer_object(19,o,90,90));
    level_cache_stream_capture_sites();
    bool archived=false;
    for(int i=0;i<sites[0].object_count;i++)if(sites[0].objects[i].value.site_token==moved_token) {
        assert(sites[0].objects[i].depth==19);archived=true;
    }
    assert(archived&&!token_obj(moved_token));
    level_image* lower=find_floor(19);assert(lower);
    int copies=0;
    for(int i=1;i<lower->o_max;i++)copies+=lower->objects[i].k_idx&&lower->objects[i].site_token==moved_token;
    assert(copies==1);
    for(int i=0;i<6;i++)shift(0,44);
    for(int i=0;i<6;i++)shift(0,-44);
    lower=find_floor(19);copies=0;
    for(int i=1;i<lower->o_max;i++)copies+=lower->objects[i].k_idx&&lower->objects[i].site_token==moved_token;
    assert(copies==1&&!token_obj(moved_token));
    puts("PASS site-owned object transferred to adjacent depth remains there through horizontal eviction/revisit without duplicate");
}
static void test_save(void)
{
    fixture_scene();
    for(int i=0;i<6;i++)shift(0,44);
    fixture_write_begin(encoded,sizeof(encoded));
    save_write_partition_meta();save_write_dungeon();level_cache_write();level_stream_write();
    save_wr_u32b(0x19ABCDEF);save_wr_u32b(0x12345678);size_t length=fixture_write_end();
    level_cache_clear();level_stream_clear();
    fixture_clear_live_lists();
    r_info[403].cur_num=0;
    fixture_read_begin(encoded,length,VERSION_EXTRA);
    assert(!load_read_partition_meta());assert(!load_read_dungeon());
    assert(!level_cache_read());assert(!level_stream_read());
    u32b a,b;load_rd_u32b(&a);load_rd_u32b(&b);
    assert(a==0x19ABCDEF&&b==0x12345678&&fixture_read_end()==length);
    assert(site_count==1&&sites[0].actor_count==2&&level_stream_get_frame().x==176);
    assert(r_info[403].cur_num>=1&&!level_cache_stream_actor_present(unique_token));
    character_dungeon=true;
    for(int i=0;i<6;i++)shift(0,-44);
    verify_site(true);
    puts("PASS real current dungeon/cache/archive byte lanes reload while hall is outside the resident horizon, then restore mutations/death");
}
static void test_timers(void)
{
    fixture_scene();
    level_image* parked=find_floor(19);assert(parked);
    parked->cells[90][90]=(environment_cell){.base_feat=FEAT_FLOOR,.integrity=100,.due=1350};
    shift(0,44);shift(0,44); /* The hall now partly overlaps the left edge. */
    turn+=100;playerturn+=50;
    level_cache_stream_capture_sites();level_cache_stream_capture_sites();
    shift(0,-44);shift(0,-44);
    verify_site(true);
    parked=find_floor(19);assert(parked->cells[90][90].due==1450);
    for(int i=0;i<6;i++)shift(0,44);
    turn+=200;playerturn+=100;
    for(int i=0;i<6;i++)shift(0,-44);
    verify_site(true);
    puts("PASS partial archived footprints and parked adjacent-floor deadlines rebase elapsed time once; repeated capture never advances frozen source/cell timers");
}
static void test_inventory(void)
{
    fixture_scene();
    int o=token_obj(moved_token);assert(o);
    inventory[0]=o_list[o];inventory[0].iy=inventory[0].ix=0;
    inventory[0].number=1;inventory[0].next_o_idx=inventory[0].held_m_idx=0;
    inventory[1]=inventory[0];delete_object_idx(o);
    p_ptr->inven_cnt=2;
    size_t length=fixture_full_write(encoded,sizeof(encoded));assert(length>0);
    assert(inventory[0].site_token&&inventory[1].site_token
        &&inventory[0].site_token!=inventory[1].site_token);
    u64b a=inventory[0].site_token,b=inventory[1].site_token;
    level_cache_clear();level_stream_clear();
    assert(!fixture_full_read(encoded,length));
    /* Loading legitimately combines identical food in the Supplies store. */
    int quantity=0;
    for(int i=0;i<INVEN_TOTAL;i++)if(inventory[i].k_idx
        &&(inventory[i].site_token==a||inventory[i].site_token==b))quantity+=inventory[i].number;
    for(int i=0;i<supplies_entry_count();i++) {
        const object_type* o=supplies_entry_at(i);
        if(o&&o->k_idx&&(o->site_token==a||o->site_token==b))quantity+=o->number;
    }
    assert(quantity==2);
    level_cache_stream_capture_sites();
    for(int i=0;i<sites[0].object_count;i++)
        assert(sites[0].objects[i].value.site_token!=a&&sites[0].objects[i].value.site_token!=b);
    puts("PASS full production save normalizes split ownership before inventory serialization; full loader preserves quantity through Supplies merge and prevents archived loot duplication");
}
static void test_malformed(void)
{
    fixture_scene();level_cache_stream_capture_sites();
    log_set_level(LOG_FATAL); /* Expected EOF diagnostics are deliberately quiet. */
    fixture_write_begin(encoded,sizeof(encoded));level_stream_write();size_t length=fixture_write_end();
    level_cache_clear();level_stream_clear();
    const size_t cuts[]={0,1,3,20,100};
    for(unsigned i=0;i<sizeof(cuts)/sizeof(*cuts);i++) {
        fixture_read_begin(encoded,cuts[i],VERSION_EXTRA);
        assert(level_stream_read()!=0);fixture_read_end();assert(site_count==0);
    }
    fixture_read_begin(encoded,length-1,VERSION_EXTRA);
    assert(level_stream_read()!=0);fixture_read_end();assert(site_count==0);
    byte changed[4];memcpy(changed,encoded,4);changed[0]^=0x40;
    fixture_read_begin(changed,sizeof(changed),VERSION_EXTRA);
    assert(level_stream_read()!=0);fixture_read_end();assert(site_count==0);
    puts("PASS malformed archive magic and truncated archive prefixes/tail fail safely without partial persistent sites");
}
static void pair_shift(int dy,int dx)
{
    p_ptr->py=p_ptr->px=110;
    s32b t=turn,pt=playerturn;
    assert(level_cache_rebase_window(dy,dx));
    assert(turn==t&&playerturn==pt&&!job);
}
static void fixture_feud(int a,int b)
{
    int members[]={a,b};
    for(int n=0;n<2;n++) {
        monster_type* m=&mon_list[members[n]];
        m->social_state=MON_SOCIAL_FIGHT;m->social_rival=members[1-n];
        m->social_memory=5;m->social_timer=4;m->social_focus=m->social_ally=0;
        m->social_cooldown=m->social_player_threat=0;
        assert(monster_social_valid(m));
    }
}
static void fixture_assert_feud(int a,int b)
{
    assert(mon_list[a].r_idx&&mon_list[b].r_idx);
    assert(monster_social_valid(&mon_list[a])&&monster_social_valid(&mon_list[b]));
    assert(mon_list[a].social_state==MON_SOCIAL_FIGHT&&mon_list[b].social_state==MON_SOCIAL_FIGHT);
    assert(mon_list[a].social_rival==b&&mon_list[b].social_rival==a);
    assert(mon_list[a].social_memory==5&&mon_list[b].social_memory==5);
    assert(mon_list[a].social_timer==4&&mon_list[b].social_timer==4);
}
static void paired_overlap_social(void)
{
    int site_pair[2]={0},ordinary_pair[2]={0},ns=0,no=0;
    level_cache_stream_capture_sites();
    for(int i=1;i<mon_max;i++)if(mon_list[i].r_idx&&mon_list[i].fx<177) {
        if(mon_list[i].site_token&&ns<2)site_pair[ns++]=i;
        else if(!mon_list[i].site_token&&no<2)ordinary_pair[no++]=i;
    }
    if(ns<2) {
        int race=0;
        for(int r=1;r<z_info->r_max;r++)if(r_info[r].name&&!(r_info[r].flags1&RF1_UNIQUE)
            &&!(r_info[r].flags2&RF2_MULTIPLY)&&r_info[r].level<=10){race=r;break;}
        for(int y=1;y<40&&ns<2;y++)for(int x=1;x<20&&ns<2;x++)
            if(cave_feat[y][x]==FEAT_FLOOR&&!cave_m_idx[y][x]&&site_at(active,y,x)
                &&place_monster_one(y,x,race,true,false,NULL))site_pair[ns++]=cave_m_idx[y][x];
        level_cache_stream_capture_sites();
    }
    assert(ns==2&&no==2);
    fixture_feud(site_pair[0],site_pair[1]);fixture_feud(ordinary_pair[0],ordinary_pair[1]);
    pair_shift(0,-44);
    fixture_assert_feud(site_pair[0],site_pair[1]);fixture_assert_feud(ordinary_pair[0],ordinary_pair[1]);
    pair_shift(0,44);
    fixture_assert_feud(site_pair[0],site_pair[1]);fixture_assert_feud(ordinary_pair[0],ordinary_pair[1]);
    for(int i=1;i<mon_max;i++)if(mon_list[i].r_idx&&mon_list[i].social_state==MON_SOCIAL_NONE)
        assert(!mon_list[i].social_rival&&!mon_list[i].social_focus&&!mon_list[i].social_ally);
    puts("PASS zero social references stay absent; valid ordinary and site-owned mutual feuds retain identities/memory/timers over overlap rebases");
}
static void test_paired(void)
{
    level_cache_clear();level_stream_clear();
    fixture_fresh(7,LEVEL_STREAM_SIZE);floors[2]=fixture_capture();
    for(int d=9;d>=8;d--) {
        fixture_fresh(d,LEVEL_STREAM_SIZE);
        vertical_vault_state pair={.count=1};
        pair.placements[0]=(vertical_vault_placement){.identity=0x81525001,
            .upper_template=525,.lower_template=526,.upper_depth=8,.y=32,.x=32,.transform=5};
        vertical_vault_begin(d,false);vertical_vault_inherit(&pair,d);
        cave_vertical_generation_begin(false);
        assert(vertical_vault_place());assert(cave_vertical_generation_finish());
        vertical_vault_capture(&pair);
        pair.placements[0].discovered_layers=d==8?1:2;
        pair.placements[0].reward_layers=d==8?1:2;
        vertical_vault_restore(&pair);
        for(int y=32;y<71;y++)for(int x=32;x<55;x++)cave_info[y][x]|=CAVE_ICKY|CAVE_G_VAULT|CAVE_MARK;
        level_image* im=fixture_capture();
        im->stream_site_count=1;
        im->stream_sites[0]=(level_stream_site_def){.y1=32,.x1=32,.y2=70,.x2=54,.kind=1};
        SDL_strlcpy(im->stream_sites[0].name,"Paired fixture",80);
        floors[d==8?0:1]=im;
        level_stream_capture_sites(im);
    }
    active=floors[0];level_image_restore(active);running=true;
    requested_depth=pending_depth=-1;
    pair_shift(44,44);
    for(int d=8;d<=9;d++) {
        const level_image* im=find_floor(d);
        assert(im&&im->vertical_vaults.count==1);
        const vertical_vault_placement* p=&im->vertical_vaults.placements[0];
        assert(p->y==-12&&p->x==-12&&p->reward_layers==3&&p->discovered_layers==3);
    }
    paired_overlap_social();
    /* A still-overlapping module has a signed origin in the real persisted
     * cache lane, even though its visible grids use compact positive locals. */
    for(int i=1;i<mon_max;i++)if(mon_list[i].r_idx&&!monster_social_valid(&mon_list[i]))
        fprintf(stderr,"invalid pre-save social monster=%d race=%d group=%u rival=%d state=%u focus=%d ally=%d memory=%u timer=%u\n",
            i,mon_list[i].r_idx,mon_list[i].social_group,mon_list[i].social_rival,
            mon_list[i].social_state,mon_list[i].social_focus,mon_list[i].social_ally,
            mon_list[i].social_memory,mon_list[i].social_timer);
    for(int i=1;i<mon_max;i++)if(mon_list[i].r_idx&&mon_list[i].social_rival>0) {
        int rival=mon_list[i].social_rival;
        if(rival<mon_max&&(mon_list[rival].social_rival!=i
            ||mon_list[rival].social_state!=mon_list[i].social_state))
            fprintf(stderr,"pre-save non-mutual feud %d ->%d state=%u vs rival->%d state=%u race=%d\n",
                i,rival,mon_list[i].social_state,mon_list[rival].social_rival,
                mon_list[rival].social_state,mon_list[rival].r_idx);
    }
    fixture_write_begin(encoded,sizeof(encoded));
    save_write_partition_meta();save_write_dungeon();level_cache_write();level_stream_write();
    save_wr_u32b(0x6E675631);save_wr_u32b(0xB17E5A11);size_t length=fixture_write_end();
    level_cache_clear();level_stream_clear();fixture_clear_live_lists();
    fixture_read_begin(encoded,length,VERSION_EXTRA);
    assert(!load_read_partition_meta());assert(!load_read_dungeon());
    assert(!level_cache_read());assert(!level_stream_read());
    u32b sentinel,tail;load_rd_u32b(&sentinel);load_rd_u32b(&tail);
    assert(sentinel==0x6E675631&&tail==0xB17E5A11&&fixture_read_end()==length);
    assert(active&&active->depth==8&&find_floor(7)&&find_floor(9));
    for(int d=8;d<=9;d++) {
        const level_image* im=find_floor(d);
        assert(im&&im->vertical_vaults.count==1);
        const vertical_vault_placement* p=&im->vertical_vaults.placements[0];
        assert(p->y==-12&&p->x==-12&&p->reward_layers==3&&p->discovered_layers==3);
    }
    puts("PASS resident paired-vault dungeon/cache/archive save roundtrip preserves negative origins and both following sentinel words");
    for(int i=0;i<6;i++)pair_shift(0,44);
    for(int i=0;i<6;i++)pair_shift(0,-44);
    pair_shift(-44,-44);
    for(int d=8;d<=9;d++) {
        const level_image* im=find_floor(d);
        assert(im&&im->vertical_vaults.count==1);
        const vertical_vault_placement* p=&im->vertical_vaults.placements[0];
        assert(p->y==32&&p->x==32&&p->reward_layers==3&&p->discovered_layers==3);
    }
    puts("PASS production paired vault module: partial negative origins, full horizontal eviction/revisit, shared discovery and consumed reward bits survive on both depths");
}
static void camera_shift(int py,int px,int dy,int dx)
{
    p_ptr->py=py;p_ptr->px=px;
    p_ptr->wy=py-SCREEN_HGT/2;p_ptr->wx=px-SCREEN_WID/2;
    verify_panel();
    int row=py-p_ptr->wy,col=px-p_ptr->wx;
    level_stream_frame frame=level_stream_get_frame();
    s64b top=frame.y+p_ptr->wy,left=frame.x+p_ptr->wx;
    assert(level_cache_rebase_window(dy,dx));
    level_stream_frame next=level_stream_get_frame();
    if(p_ptr->py-p_ptr->wy!=row||p_ptr->px-p_ptr->wx!=col)
        fprintf(stderr,"camera shift %d,%d oldplayer=%d,%d screen=%d,%d -> %d,%d newcamera=%d,%d expectedworld=%lld,%lld actualworld=%lld,%lld\n",
            dy,dx,py,px,row,col,p_ptr->py-p_ptr->wy,p_ptr->px-p_ptr->wx,p_ptr->wy,p_ptr->wx,
            (long long)top,(long long)left,(long long)(next.y+p_ptr->wy),(long long)(next.x+p_ptr->wx));
    assert(p_ptr->py-p_ptr->wy==row&&p_ptr->px-p_ptr->wx==col);
    assert(next.y+p_ptr->wy==top&&next.x+p_ptr->wx==left);
    assert(!modify_panel(p_ptr->wy,p_ptr->wx));
    assert(p_ptr->py>=p_ptr->wy&&p_ptr->py<p_ptr->wy+SCREEN_HGT);
    assert(p_ptr->px>=p_ptr->wx&&p_ptr->px<p_ptr->wx+SCREEN_WID);
}
static void test_camera(void)
{
    fixture_scene();
    op_ptr->opt[OPT_center_player]=false;
    camera_shift(110,110,44,44);
    camera_shift(110,110,-44,-44);
    camera_shift(52,80,44,44);
    camera_shift(110,110,-44,-44);
    /* Oversized stale coordinates must also clamp legally on verify. */
    p_ptr->py=p_ptr->px=110;p_ptr->wy=p_ptr->wx=10000;
    verify_panel();assert(!modify_panel(p_ptr->wy,p_ptr->wx));
    assert(p_ptr->py-p_ptr->wy>=0&&p_ptr->py-p_ptr->wy<SCREEN_HGT);
    assert(p_ptr->px-p_ptr->wx>=0&&p_ptr->px-p_ptr->wx<SCREEN_WID);
    puts("PASS valid centered camera preserves player screen position and world viewport on positive/negative rebases; near-guard and stale-camera clamps remain legal");
}
static void wait_worker_floor(int depth)
{
    if(find_floor(depth))return;
    assert(level_cache_request(depth)||job);
    Uint64 deadline=SDL_GetTicks()+60000;
    int completions=0;
    while(!find_floor(depth)&&SDL_GetTicks()<deadline) {
        if(job&&SDL_GetAtomicInt(&job->done)&&completions++<8)
            printf("worker completion depth=%d persistent=%d success=%d cancel=%d conflict=%d player=%u,%u\n",
                job->depth,job->persistent_depth,job->success,SDL_GetAtomicInt(&job->cancel),
                job->success?generation_conflicts(job):0,job->player.py,job->player.px);
        level_cache_poll();
        if(!find_floor(depth))SDL_Delay(1);
    }
    if(!find_floor(depth))fprintf(stderr,"worker deadline depth=%d active=%d job=%d done=%d requested=%d pending=%d\n",
        depth,active?active->depth:-1,job?job->depth:-1,job?SDL_GetAtomicInt(&job->done):-1,
        requested_depth,pending_depth);
    assert(find_floor(depth));
}
static void travel_worker_floor(int depth)
{
    wait_worker_floor(depth);
    const level_image* target=find_floor(depth);
    assert(target&&target->py>0&&target->px>0);
    int y=target->py,x=target->px;
    s32b t=turn,pt=playerturn;
    assert(level_cache_travel_to(depth,y,x));
    assert(level_cache_change_level());
    assert(active&&active->depth==depth&&p_ptr->depth==depth);
    assert(turn==t&&playerturn==pt);
}
static void stop_prefetch(void)
{
    if(job) {
        SDL_SetAtomicInt(&job->cancel,1);SDL_WaitThread(job->thread,NULL);
        free_job(job);job=NULL;
    }
}
static void test_worker(void)
{
    level_cache_clear();level_stream_clear();fixture_fresh(20,LEVEL_STREAM_SIZE);
    Rand_state_init(7094);
    puts("Generating actual initial depth20 seed7094");
    generate_cave();
    assert(character_dungeon&&p_ptr->depth==20);
    floors[0]=active=fixture_capture();running=true;requested_depth=pending_depth=-1;
    level_cache_stream_capture_sites();
    assert(level_stream_has_sites(20));
    int initial_morgoths=0;
    for(int i=1;i<mon_max;i++)initial_morgoths+=mon_list[i].r_idx==R_IDX_MORGOTH;
    assert(initial_morgoths==1);
    int pin_y=0,pin_x=0;
    for(int y=1;y<p_ptr->cur_map_hgt-1&&!pin_y;y++)for(int x=1;x<p_ptr->cur_map_wid-1;x++)
        if((cave_info[y][x]&CAVE_ICKY)&&site_at(active,y,x)&&!cave_m_idx[y][x]&&!cave_o_idx[y][x]
            &&(cave_feat[y][x]==FEAT_WALL_INNER||cave_feat[y][x]==FEAT_WALL_OUTER)) {
            pin_y=y;pin_x=x;break;
        }
    assert(pin_y);
    cave_set_feat(pin_y,pin_x,FEAT_DAMAGED_WALL);
    level_stream_frame origin=level_stream_get_frame();
    s64b world_y=origin.y+pin_y-1,world_x=origin.x+pin_x-1;
    level_cache_stream_capture_sites();
    puts("Generating real cache workers and travelling20 ->19 ->18");
    travel_worker_floor(19);travel_worker_floor(18);
    assert(!find_floor(20)&&level_stream_has_sites(20));
    stop_prefetch();
    puts("Regenerating actual persistent-site depth20 in background worker");
    travel_worker_floor(20);stop_prefetch();
    level_stream_frame returned=level_stream_get_frame();
    int y=(int)(world_y-returned.y+1),x=(int)(world_x-returned.x+1);
    assert(in_bounds_fully(y,x)&&cave_feat[y][x]==FEAT_DAMAGED_WALL);
    assert(level_stream_has_sites(20));
    int morgoths=0;
    for(int s=0;s<LEVEL_CACHE_SLOTS;s++)if(floors[s])for(int m=1;m<floors[s]->mon_max;m++)
        morgoths+=floors[s]->monsters[m].r_idx==R_IDX_MORGOTH;
    assert(morgoths==initial_morgoths);
    puts("PASS real generate_cave depth20, asynchronous neighbor generation/travel to19/18, full depth20 eviction, real worker ordinary regeneration with pinned authored mutation restored and unique not duplicated");
}
static void test_legacy(void)
{
    level_cache_clear();level_stream_clear();fixture_fresh(20,88);
    level_stream_set_frame((level_stream_frame){0});
    cave_feat[70][70]=FEAT_DAMAGED_WALL;cave_info[70][70]=CAVE_MARK|CAVE_WALL;
    object_at(65,65,TV_FOOD,731);
    for(int y=20;y<=30;y++)for(int x=20;x<=30;x++)cave_info[y][x]|=CAVE_ICKY|CAVE_G_VAULT|CAVE_MARK;
    cave_feat[25][25]=FEAT_DAMAGED_WALL;
    int race=0;
    for(int i=1;i<z_info->r_max;i++)if(r_info[i].name&&!(r_info[i].flags1&RF1_UNIQUE)
        &&!(r_info[i].flags2&RF2_MULTIPLY)&&r_info[i].level<=10){race=i;break;}
    assert(race&&place_monster_one(60,60,race,true,false,NULL));
    fixture_legacy_mode(true);
    fixture_write_begin(encoded,sizeof(encoded));save_write_partition_meta();save_write_dungeon();
    save_wr_u32b(0x77331155);save_wr_u32b(0x12345678);size_t length=fixture_write_end();
    fixture_legacy_mode(false);
    level_stream_clear();
    fixture_clear_live_lists();
    fixture_read_begin(encoded,length,9);
    assert(!load_read_partition_meta());assert(!load_read_dungeon());
    u32b a,b;load_rd_u32b(&a);load_rd_u32b(&b);
    assert(a==0x77331155&&b==0x12345678&&fixture_read_end()==length);
    assert(!level_stream_enabled()&&cave_feat[70][70]==FEAT_DAMAGED_WALL);
    floors[0]=active=fixture_capture();running=false; /* Adopt without starting an unrelated prefetch. */
    character_dungeon=true;level_stream_update();
    assert(level_stream_enabled());
    assert(p_ptr->cur_map_hgt==LEVEL_STREAM_SIZE&&p_ptr->cur_map_wid==LEVEL_STREAM_SIZE);
    assert(cave_feat[114][114]==FEAT_DAMAGED_WALL);
    assert(cave_info[114][114]&CAVE_MARK);
    bool found=false;for(int i=1;i<o_max;i++)if(o_list[i].k_idx&&o_list[i].pval==731)found=true;
    assert(found&&level_stream_get_frame().y==-44&&level_stream_get_frame().x==-44);
    assert(cave_m_idx[104][104]>0&&mon_list[cave_m_idx[104][104]].r_idx==race);
    assert(site_count==1);
    for(int i=0;i<6;i++)shift(0,44);
    for(int i=0;i<6;i++)shift(0,-44);
    assert(cave_feat[69][69]==FEAT_DAMAGED_WALL);
    puts("PASS exact pre-stream v1.1.0.9 monster/item layout loads without ownership tokens; finite map imports terrain/objects/actors, inferred permanent hall survives full eviction/revisit");
}
static void checks(const char* scenario)
{
    if(!strcmp(scenario,"horizon"))test_horizon();
    else if(!strcmp(scenario,"transfer"))test_transfer();
    else if(!strcmp(scenario,"save"))test_save();
    else if(!strcmp(scenario,"legacy"))test_legacy();
    else if(!strcmp(scenario,"timers"))test_timers();
    else if(!strcmp(scenario,"inventory"))test_inventory();
    else if(!strcmp(scenario,"malformed"))test_malformed();
    else if(!strcmp(scenario,"paired"))test_paired();
    else if(!strcmp(scenario,"camera"))test_camera();
    else if(!strcmp(scenario,"worker"))test_worker();
    else assert(false);
    level_cache_clear();level_stream_clear();
}
'''


def main(case):
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    for name in ("terminal_extra", "reset_map"):
        prefix += fixture_function(name) + "\n"
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    init += "    assert(!init_quest_info());assert(!init_st_info());assert(!init_partition_info());\n"
    init += "    style_info=(style_type*)style_head.info_ptr;style_name=style_head.name_ptr;\n"
    init += "    assert(!init_flavor_info());assert(!init_effect_info());assert(!init_skeleton_note_info());\n"
    init += "    assert(!init_h_info());assert(!init_n_info());build_randart_tables();flavor_init();drop_system_init();character_generated=true;\n"
    source = OUT / "check.c"
    source.write_text(prefix + TESTS + init + '    checks(getenv("STREAM_CASE"));SDL_Quit();return 0;\n}\n', encoding="utf-8")
    save_source = (ROOT / "src/fs/save.c").read_text()
    save_source = re.sub(r"^(\s*)wr_u32b\(\(u32b\)([om]_ptr)->site_token\);$",
                         r"\1if (!fixture_legacy) wr_u32b((u32b)\2->site_token);", save_source, flags=re.M)
    save_source = re.sub(r"^(\s*)wr_u32b\(\(u32b\)\(([om]_ptr)->site_token >> 32\)\);$",
                         r"\1if (!fixture_legacy) wr_u32b((u32b)(\2->site_token >> 32));", save_source, flags=re.M)
    assert save_source.count("if (!fixture_legacy)") == 4
    writer = OUT / "writer.c"
    writer.write_text('#include "angband.h"\n#include <assert.h>\nstatic bool fixture_legacy;\n'
                      'void fixture_legacy_mode(bool legacy) { fixture_legacy=legacy; }\n'
                      + save_source + WRITER[WRITER.index("void fixture_write_begin"):]
                      + '\nsize_t fixture_full_write(byte* buffer,size_t cap) { fixture_write_begin(buffer,cap); assert(wr_savefile());return fixture_write_end(); }\n', encoding="utf-8")
    reader = OUT / "reader.c"
    load_source = (ROOT / "src/fs/load.c").read_text()
    load_source = load_source.replace('void note(cptr msg)\n{',
        'void note(cptr msg)\n{\n    if (getenv("STREAM_TRACE")) fprintf(stderr,"load note: %s\\n",msg);')
    # load_player normally supplies the Varda availability flag before its
    # internal full reader; all other availability flags initialize there.
    reader.write_text(READER.replace('#include "fs/load.c"', load_source)
                      + '\nerrr fixture_full_read(const byte* buffer,size_t size) { fixture_read_begin(buffer,size,VERSION_EXTRA);savefile_has_varda_quest=true;errr result=rd_savefile_new_aux();if(result)fprintf(stderr,"full reader result=%d offset=%u/%zu\\n",result,load_byte_offset,size);fixture_read_end();return result; }\n', encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    exclude = ("/src/main.c.obj", "/src/fs/save.c.obj", "/src/fs/load.c.obj",
               "/src/level-generation/level-cache.c.obj", "/src/level-generation/level-stream-sites.c.obj",
               "/src/cave/cave-environment.c.obj", "/src/sdl/input/sdl-mouse-path.c.obj")
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects if not obj.endswith(exclude)))
    env = os.environ.copy()
    env["SDL_VIDEODRIVER"] = env["SDL_AUDIODRIVER"] = "dummy"
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g", "-UNDEBUG",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), str(writer), str(reader),
                    str(ROOT / "src/cave/cave-environment.c"),
                    str(ROOT / "src/sdl/input/sdl-mouse-path.c"),
                    "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-o", str(exe)],
                   cwd=BUILD, env=env, check=True)
    cases = ("horizon", "transfer", "save", "legacy", "timers", "inventory", "malformed", "paired", "camera", "worker") if case == "all" else (case,)
    for scenario in cases:
        with tempfile.TemporaryDirectory(prefix="data-", dir=OUT) as data:
            subprocess.run([str(exe), str(ROOT / "lib/edit"), data], cwd=data,
                           env={**env, "STREAM_CASE": scenario}, check=True, timeout=180)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case", choices=("all", "horizon", "transfer", "save", "legacy", "timers", "inventory", "malformed", "paired", "camera", "worker"), default="all")
    main(parser.parse_args().case)

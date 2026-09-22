#!/usr/bin/env python3
"""Behavioral ecology tests linked against current production engine objects.

Build with build-incremental.ps1 first. No player files are accessed.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
from check_living_dungeon_save import ENGINE_FIXTURE, fixture_function, FRESH_MAP

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build-standard'
OUT = ROOT / 'scripts/output/living-dungeon'

TESTS = r'''
static int checks, failures;
#define CHECK(t) do { checks++; if (!(t)) { fprintf(stderr,"FAIL %s:%d: %s\n",__func__,__LINE__,#t); failures++; return; } } while(0)
static void clean(void)
{
    fresh_map(); turn=1000; character_dungeon=true;
    op_ptr->environment_speed=ENVIRONMENT_SPEED_SLOW; /* Original regression baseline. */
    p_ptr->py=2;p_ptr->px=2;
    partition_meta_save meta={0};meta.grid_rows=meta.grid_cols=meta.partition_count=1;
    meta.modes[0]=QUAD_MODE_ROOMY;level_partition_meta_set(&meta);
}
static void tick(int count)
{
    for(int n=0;n<count;n++){turn+=10;cave_environment_process();}
}
static void setcell(int y,int x,int f)
{ cave_set_feat(y,x,f); }
static void test_utumno_contacts(void)
{
    for(int depth=UTUMNO_DEPTH;depth<=UTUMNO_FORGE_DEPTH;depth++) {
        clean();p_ptr->depth=depth;
        /* Four separated fronts must all react in one pulse, without an ice
         * tile's own cold cancelling heat from the adjacent molten rock. */
        for(int y=5;y<=14;y+=3) {
            setcell(y,8,FEAT_LAVA);setcell(y,9,FEAT_ICE);
        }
        cave_environment_seed();
        for(int i=0;i<cave_environment_get_state().source_count;i++) {
            environment_source source=*cave_environment_source_at(i);
            source.next_turn=turn+10000;CHECK(cave_environment_restore_source(i,source));
        }
        tick(10);
        for(int y=5;y<=14;y+=3) {
            CHECK(cave_feat[y][8]==FEAT_LAVA);
            CHECK(cave_feat[y][9]==FEAT_MELTING_ICE);
        }
        tick(10);
        for(int y=5;y<=14;y+=3)CHECK(cave_feat[y][9]==FEAT_WATER);
        tick(10);
        for(int y=5;y<=14;y+=3) {
            CHECK(cave_feat[y][8]==FEAT_FLOOR);
            CHECK(cave_environment_cell_at(y,8)->flags&ENV_DEPOSIT);
        }
        /* Neither occupied ice nor a protected dry destination may change. */
        clean();p_ptr->depth=depth;
        setcell(10,10,FEAT_LAVA);setcell(10,11,FEAT_ICE);
        cave_m_idx[10][11]=-1;p_ptr->py=10;p_ptr->px=11;
        setcell(9,10,FEAT_MORE);cave_environment_seed();tick(50);
        CHECK(cave_feat[10][11]==FEAT_ICE && cave_feat[9][10]==FEAT_MORE);
        clean();p_ptr->depth=depth;
        setcell(10,10,FEAT_WATER);
        setcell(9,10,FEAT_ICE);setcell(11,10,FEAT_ICE);
        setcell(10,9,FEAT_ICE);setcell(10,11,FEAT_ICE);
        cave_environment_seed();tick(10);
        CHECK(cave_feat[10][10]==FEAT_ICE);
        clean();p_ptr->depth=depth;
        setcell(10,10,FEAT_LAVA);setcell(10,11,FEAT_WATER);
        cave_environment_seed();
        environment_cell added=*cave_environment_cell_at(10,10);
        added.flags|=ENV_ADDED_LIQUID;added.base_feat=FEAT_FLOOR;
        CHECK(cave_environment_restore_cell(10,10,added));
        environment_source source=*cave_environment_source_at(added.owner-1);
        source.used=1;source.next_turn=turn+10000;
        CHECK(cave_environment_restore_source(added.owner-1,source));
        tick(10);
        CHECK(cave_feat[10][10]==FEAT_FLOOR);
        CHECK(cave_environment_source_at(added.owner-1)->used==0);
        CHECK(!(cave_environment_cell_at(10,10)->flags&ENV_ADDED_LIQUID));
    }
    puts("Utumno: four concurrent ice/lava fronts thaw in stages, water quenches lava, forge simulation runs, occupied ice/stairs survive PASS.");
}
static void test_environment_speed(void)
{
    CHECK(op_ptr->environment_speed==ENVIRONMENT_SPEED_NORMAL); /* Production init. */
    const int feats[]={FEAT_WATER,FEAT_LAVA,FEAT_POISON,FEAT_CHASM};
    const int intervals[]={200,350,600,1200};
    int initial[4]={0};
    for(int speed=ENVIRONMENT_SPEED_SLOW;speed<=ENVIRONMENT_SPEED_FAST;speed++) {
        int rate=1<<speed;
        for(int f=0;f<4;f++) {
            clean();op_ptr->environment_speed=speed;setcell(10,10,feats[f]);
            cave_environment_seed();CHECK(cave_environment_get_state().source_count==1);
            environment_source source=*cave_environment_source_at(0);
            if(!speed)initial[f]=source.next_turn-turn;
            CHECK(source.next_turn-turn==(initial[f]+rate-1)/rate);
            source.next_turn=turn;source.phase=0;CHECK(cave_environment_restore_source(0,source));
            int started=turn;
            while(!cave_environment_source_at(0)->phase && turn<started+110)tick(1);
            CHECK(turn-started==(((100+rate-1)/rate+9)/10)*10);
            CHECK(cave_environment_source_at(0)->next_turn-turn==(intervals[f]+rate-1)/rate);
        }
        clean();op_ptr->environment_speed=speed;
        setcell(10,10,FEAT_BRIDGE_WATER_H);cave_environment_seed();
        cave_environment_flood_bridge(10,10,FEAT_WATER,100);
        CHECK(cave_environment_pending_hazard(10,10)==FEAT_WATER);
        CHECK(cave_environment_cell_at(10,10)->due==turn+50);
    }
    clean();setcell(10,10,FEAT_WATER);cave_environment_seed();
    environment_source source=*cave_environment_source_at(0);source.next_turn=turn+400;
    CHECK(cave_environment_restore_source(0,source));
    environment_cell cell=*cave_environment_cell_at(10,11);
    cell.pending_feat=FEAT_WATER;cell.due=turn+50;CHECK(cave_environment_restore_cell(10,11,cell));
    u32b random=cave_environment_get_state().random;
    cave_environment_set_speed(ENVIRONMENT_SPEED_NORMAL);
    CHECK(cave_environment_source_at(0)->next_turn==turn+200);
    cave_environment_set_speed(ENVIRONMENT_SPEED_FAST);
    CHECK(cave_environment_source_at(0)->next_turn==turn+100);
    cave_environment_set_speed(ENVIRONMENT_SPEED_FAST);
    CHECK(cave_environment_source_at(0)->next_turn==turn+100);
    cave_environment_set_speed(ENVIRONMENT_SPEED_SLOW);
    CHECK(cave_environment_source_at(0)->next_turn==turn+400);
    CHECK(cave_environment_cell_at(10,11)->due==turn+50);
    CHECK(cave_environment_get_state().random==random);
    CHECK(cave_environment_get_state().last_turn==turn);
    puts("Environment speed: faster water/lava cadence, Normal 2x, Fast 4x, live rescheduling and unchanged warnings PASS.");
}

bool test_pick_environment_speed(bool* handled);
void test_reset_environment_speed(bool* app_dirty);
static int speed_choice;
int __wrap_ui_question_ask_overlay_buttons(cptr title,cptr desc,
    const ui_question_option* options,int count,const ui_question_button* buttons,
    int button_count,int y,int x,int selected)
{
    (void)desc;(void)buttons;(void)button_count;(void)y;(void)x;
    if(!strcmp(title,"Environmental effects speed")) {
        assert(count==3 && selected==op_ptr->environment_speed);
        assert(!strcmp(options[0].label,"Slow"));
        assert(!strcmp(options[1].label,"Normal (default)"));
        assert(!strcmp(options[2].label,"Fast"));
        return speed_choice;
    }
    return -1;
}
static void test_environment_speed_settings(void)
{
    const char* path="environment-speed-settings.json";
    CHECK(option_is_app_persistent(OPT_environment_speed));
    bool listed=false;
    for(int i=0;i<OPT_PAGE_PER;i++)if(option_page[GAMEPLAY_PAGE][i]==OPT_environment_speed)listed=true;
    CHECK(listed);
    const char* invalid[]={"{}","{\"appOptions\":{\"gameplay\":{\"environmentSpeed\":99}}}",
        "{\"appOptions\":{\"gameplay\":{\"environmentSpeed\":-1}}}",
        "{\"appOptions\":{\"gameplay\":{\"environmentSpeed\":true}}}",
        "{\"appOptions\":{\"gameplay\":{\"environmentSpeed\":1.5}}}"};
    for(unsigned i=0;i<N_ELEMENTS(invalid);i++) {
        FILE* file=fopen(path,"wb");CHECK(file);fputs(invalid[i],file);fclose(file);
        op_ptr->environment_speed=ENVIRONMENT_SPEED_FAST;sdl_config_load_app_options(path);
        CHECK(op_ptr->environment_speed==ENVIRONMENT_SPEED_NORMAL);
    }
    bool handled=false,app_dirty=false;
    for(int speed=0;speed<=ENVIRONMENT_SPEED_FAST;speed++) {
        speed_choice=speed;
        CHECK(test_pick_environment_speed(&handled) && handled);
        CHECK(op_ptr->environment_speed==speed);
        CHECK(sdl_config_save(path,&config,NULL,0));
        op_ptr->environment_speed=99;sdl_config_load_app_options(path);
        CHECK(op_ptr->environment_speed==speed);
    }
    speed_choice=-1;CHECK(!test_pick_environment_speed(&handled));
    CHECK(op_ptr->environment_speed==ENVIRONMENT_SPEED_FAST);
    test_reset_environment_speed(&app_dirty);
    CHECK(app_dirty && op_ptr->environment_speed==ENVIRONMENT_SPEED_NORMAL);
    remove(path);
    puts("Environment speed settings: Gameplay menu, three picker choices/cancel/reset, Normal default, invalid/old JSON and persistence PASS.");
}
static int wizard_menu_selections;
int __wrap_ui_question_ask(cptr title,cptr desc,const ui_question_option* options,
    int count,int y,int x,int selected)
{
    (void)desc;(void)y;(void)x;(void)selected;
    cptr label=!strcmp(title,"Debug Commands") ? "Map and Travel"
        : !strcmp(title,"Map and Travel") ? "Teleport to last dungeon event (h)" : NULL;
    if(label)for(int i=0;i<count;i++)if(!strcmp(options[i].label,label)) {
        wizard_menu_selections++;
        return i;
    }
    return -1;
}
static void test_wizard_event_travel(void)
{
    clean();cave_m_idx[2][2]=-1;op_ptr->opt[OPT_show_dungeon_events]=false;
    do_cmd_debug();
    CHECK(wizard_menu_selections==2 && p_ptr->py==2 && p_ptr->px==2);
    cave_event_emit(CAVE_EVENT_BUILD,8,9,8);
    cave_event_emit(CAVE_EVENT_DIG,8,8,8);turn+=600;
    p_ptr->leaping=true;
    do_cmd_debug();
    CHECK(wizard_menu_selections==4 && p_ptr->py==8 && p_ptr->px==8);
    CHECK(cave_m_idx[2][2]==0 && cave_m_idx[8][8]==-1 && !p_ptr->leaping);
    do_cmd_debug();CHECK(p_ptr->py==8 && p_ptr->px==8);
    setcell(12,12,FEAT_LAVA);cave_environment_seed();
    CHECK(place_monster_one(11,12,31,false,false,NULL));
    int mon=cave_m_idx[11][12];
    environment_cell warning=*cave_environment_cell_at(12,13);
    warning.pending_feat=FEAT_LAVA;warning.due=turn+50;
    CHECK(cave_environment_restore_cell(12,13,warning));
    cave_event_emit(CAVE_EVENT_VENT,12,12,12);
    do_cmd_debug();
    CHECK(distance(p_ptr->py,p_ptr->px,12,12)==1);
    CHECK(cave_feat[p_ptr->py][p_ptr->px]==FEAT_FLOOR);
    CHECK(!cave_environment_pending_hazard(p_ptr->py,p_ptr->px));
    CHECK(cave_m_idx[11][12]==mon && cave_m_idx[8][8]==0);
    CHECK(cave_m_idx[p_ptr->py][p_ptr->px]==-1);
    int py=p_ptr->py,px=p_ptr->px;
    for(int y=1;y<19;y++)for(int x=1;x<23;x++)
        if(y!=py||x!=px)setcell(y,x,FEAT_WALL_PERM);
    cave_event_emit(CAVE_EVENT_CRACK,12,12,12);
    do_cmd_debug();CHECK(p_ptr->py==py && p_ptr->px==px);
    cave_events_reset();do_cmd_debug();CHECK(p_ptr->py==py && p_ptr->px==px);
    puts("Wizard menu: latest expired/offscreen event, exact/safe landing, occupied/warning avoidance and no-event/no-floor cases PASS.");
}
static void test_debug_options(void)
{
    const char* path="dungeon-event-settings.json";
    CHECK(option_is_app_persistent(OPT_show_dungeon_events));
    sdl_config_reset_app_options_to_defaults();
    CHECK(!op_ptr->opt[OPT_show_dungeon_events]);
    FILE* file=fopen(path,"wb");CHECK(file);
    fputs("{\"appOptions\":{\"interface\":{\"auto_more\":true}}}",file);fclose(file);
    op_ptr->opt[OPT_show_dungeon_events]=true;
    sdl_config_load_app_options(path);
    CHECK(!op_ptr->opt[OPT_show_dungeon_events]); /* No opt-in from an old key. */
    for(int enabled=0;enabled<=1;enabled++) {
        op_ptr->opt[OPT_show_dungeon_events]=enabled;
        CHECK(sdl_config_save(path,&config,NULL,0));
        op_ptr->opt[OPT_show_dungeon_events]=!enabled;
        sdl_config_load_app_options(path);
        CHECK(op_ptr->opt[OPT_show_dungeon_events]==enabled);
    }
    /* The old metarun slot must not override the app preference. */
    metar.persistent_options_initialized=1;
    metar.persistent_options[OPT_show_dungeon_events/32]=0;
    metarun_load_persistent_settings();
    CHECK(op_ptr->opt[OPT_show_dungeon_events]);
    sdl_config_reset_app_options_to_defaults();
    CHECK(!op_ptr->opt[OPT_show_dungeon_events]);
    remove(path);
    puts("Debug event setting: default off, legacy key ignored, JSON on/off and metarun isolation PASS.");
}
static void test_memory(void)
{
    clean();setcell(8,8,FEAT_WATER);
    cave_info[8][8]=CAVE_MARK; cave_environment_seed();
    byte expected_a,a;char expected_c,c;
    map_info_terrain(8,8,&expected_a,&expected_c);
    setcell(8,8,FEAT_POISON);
    CHECK(cave_environment_known_feature(8,8)==FEAT_WATER);
    CHECK(cave_environment_display_underlay(8,8)==FEAT_WATER);
    map_info_terrain(8,8,&a,&c);
    CHECK(a==expected_a && c==expected_c);
    cave_info[8][8]|=CAVE_SEEN;note_spot(8,8);
    CHECK(cave_environment_known_feature(8,8)==FEAT_POISON);
    cave_info[8][8]&=~CAVE_SEEN;
    CHECK(cave_environment_known_feature(8,8)==FEAT_POISON);
    puts("Memory: unseen change keeps the last observed terrain and underlay PASS.");
}
static void test_vents(void)
{
    clean();p_ptr->depth=15;
    partition_meta_save meta={0};meta.grid_rows=meta.grid_cols=meta.partition_count=1;
    meta.modes[0]=QUAD_MODE_BIG_CAVE;meta.big_cave_types[0]=BIG_CAVE_FIRE;
    level_partition_meta_set(&meta);
    for(int y=8;y<14;y+=2) {
        setcell(y,16,FEAT_WALL_EXTRA);setcell(y,18,FEAT_WALL_EXTRA);
    }
    cave_environment_seed();
    CHECK(cave_environment_get_state().source_count>0);
    bool opened=false;
    for(int i=0;i<600;i++) {
        tick(1);
        for(int y=1;y<p_ptr->cur_map_hgt-1;y++)for(int x=1;x<p_ptr->cur_map_wid-1;x++)
            opened |= cave_feat[y][x]==FEAT_LAVA;
    }
    CHECK(opened);
    puts("Vents: a buried fire-cave source opens a new finite lava pool PASS.");
}
static void test_clock(void)
{
    clean();cave_environment_seed();
    for(int x=5;x<11;x++) {
        environment_cell c=*cave_environment_cell_at(8,x);
        c.pending_feat=FEAT_WATER;c.due=turn;
        CHECK(cave_environment_restore_cell(8,x,c));
    }
    cave_environment_process();
    byte snapshot[6];for(int x=5;x<11;x++)snapshot[x-5]=cave_feat[8][x];
    environment_state before=cave_environment_get_state();
    for(int n=0;n<5;n++)cave_environment_process();
    CHECK(cave_environment_get_state().random==before.random);
    for(int x=5;x<11;x++)CHECK(snapshot[x-5]==cave_feat[8][x]);
    puts("Clock: repeated same-tick processing does not advance pending work PASS.");
}
static void test_reservoir(void)
{
    clean();setcell(10,10,FEAT_WATER);setcell(10,11,FEAT_WALL_EXTRA);
    cave_environment_seed();
    CHECK(cave_environment_get_state().source_count==1);
    bool rose=false,fell=false,eroded=false;int previous=0;
    for(int n=0;n<2200;n++) {
        tick(1);
        const environment_source* s=cave_environment_source_at(0);
        CHECK(s->used<=s->capacity && s->capacity<=24);
        if(s->used>previous)rose=true;if(rose&&s->used<previous)fell=true;
        previous=s->used;
        if(cave_feat[10][11]!=FEAT_WALL_EXTRA)eroded=true;
    }
    CHECK(rose&&fell&&eroded);
    puts("Reservoir: real seeded water rises/recedes within supply; bank erodes PASS.");
}
static void test_thermal(void)
{
    clean();
    for(int y=8;y<=12;y++)for(int x=8;x<=12;x++)setcell(y,x,FEAT_ICE);
    setcell(10,10,FEAT_FLOOR);cave_natural[10][10]=1;
    cave_environment_seed();
    tick(200);CHECK(FEAT_IS_ICE(cave_feat[10][10]));
    CHECK(cave_environment_cell_at(10,10)->flags&ENV_FLOOR_ICE);
    CHECK(cave_environment_cell_at(7,10)->heat<0);
    for(int y=8;y<=12;y++)for(int x=8;x<=12;x++)if(y!=10||x!=10)setcell(y,x,FEAT_LAVA);
    tick(500);CHECK(cave_feat[10][10]==FEAT_FLOOR);
    CHECK(!(cave_environment_cell_at(10,10)->flags&ENV_FLOOR_ICE));
    puts("Heat: influence spreads beyond origin; frozen solid floor thaws to floor PASS.");
}
static void test_floor_chasm_bridges(void)
{
    clean();op_ptr->environment_speed=ENVIRONMENT_SPEED_NORMAL;
    for(int y=9;y<=11;y++) {
        setcell(y,9,FEAT_CHASM);setcell(y,10,FEAT_FLOOR);setcell(y,11,FEAT_CHASM);
        cave_info[y][10]|=CAVE_CHASM_AREA;
    }
    cave_environment_seed();
    environment_state state=cave_environment_get_state();
    CHECK(state.source_count==2);
    CHECK(cave_environment_cell_at(10,10)->owner>0);
    environment_cell bridge=*cave_environment_cell_at(10,10);
    bridge.owner=0;CHECK(cave_environment_restore_cell(10,10,bridge));
    CHECK(cave_environment_cell_at(10,10)->owner==0);
    for(int i=0;i<state.source_count;i++) {
        environment_source source=*cave_environment_source_at(i);
        source.next_turn=turn;source.phase=0;
        CHECK(cave_environment_restore_source(i,source));
    }
    tick(5);
    CHECK(cave_environment_cell_at(10,10)->owner>0);
    CHECK(cave_environment_cell_at(10,10)->integrity==96);
    setcell(10,10,FEAT_CHASM);
    environment_bridge_job job;
    CHECK(cave_environment_cell_at(10,10)->integrity==0);
    CHECK(cave_environment_bridge_job_at(10,10,&job));
    CHECK(job.material==ENV_BRIDGE_STONE&&job.feature==FEAT_FLOOR&&job.integrity==0);
    for(int n=0;n<15;n++)CHECK(!cave_environment_bridge_work(10,10,job.material));
    CHECK(cave_environment_bridge_work(10,10,job.material));
    CHECK(cave_feat[10][10]==FEAT_FLOOR);
    CHECK(cave_environment_cell_at(10,10)->integrity==100);
    puts("Legacy floor-over-chasm bridges: one source claim prevents stacked erosion and stone repair restores floor crossing PASS.");
}
static void bridge_fixture(void)
{
    clean();setcell(10,10,FEAT_BRIDGE_WATER_H);
    cave_environment_seed();
}
static void test_partition_heat(void)
{
    clean();partition_meta_save meta={0};meta.grid_rows=1;meta.grid_cols=2;meta.partition_count=2;
    meta.modes[0]=QUAD_MODE_BIG_CAVE;meta.big_cave_types[0]=BIG_CAVE_ICE;
    meta.modes[1]=QUAD_MODE_ROOMY;level_partition_meta_set(&meta);
    int edge=0;for(int x=2;x<22;x++)if(level_partition_index_for_point(10,x)!=level_partition_index_for_point(10,x-1)){edge=x;break;}
    CHECK(edge>0);cave_environment_seed();tick(20);
    CHECK(level_partition_big_cave_type_for_point(10,edge)==BIG_CAVE_NONE);
    CHECK(cave_environment_cell_at(10,edge)->heat<0);
    puts("Partition heat: cold crosses an actual icy-cave / ordinary-room boundary PASS.");
}
static void test_bridges(void)
{
    environment_bridge_job job;
    bridge_fixture();cave_environment_flood_bridge(10,10,FEAT_WATER,5);
    CHECK(cave_environment_bridge_job_at(10,10,&job));
    cave_m_idx[10][10]=-1;
    for(int n=0;n<3;n++)CHECK(!cave_environment_bridge_work(10,10,job.material));
    CHECK(cave_environment_bridge_work(10,10,job.material));
    CHECK(cave_environment_cell_at(10,10)->integrity==100);
    cave_m_idx[10][10]=0;
    cave_environment_flood_bridge(10,10,FEAT_LAVA,100);
    CHECK(cave_environment_pending_hazard(10,10)==FEAT_LAVA);
    CHECK(!cave_environment_bridge_job_at(10,10,&job));
    tick(6);CHECK(cave_feat[10][10]==FEAT_LAVA);
    CHECK(cave_environment_bridge_job_at(10,10,&job));
    CHECK(job.material==ENV_BRIDGE_STONE&&job.feature==FEAT_BRIDGE_LAVA_H);
    for(int n=0;n<15;n++)CHECK(!cave_environment_bridge_work(10,10,job.material));
    CHECK(cave_environment_bridge_work(10,10,job.material));
    CHECK(cave_feat[10][10]==FEAT_BRIDGE_LAVA_H);
    cave_environment_flood_bridge(10,10,FEAT_LAVA,1);
    setcell(10,9,FEAT_CHASM);CHECK(!cave_environment_bridge_job_at(10,10,&job));
    clean();setcell(10,10,FEAT_WATER);cave_environment_seed();
    CHECK(!cave_environment_bridge_work(10,10,ENV_BRIDGE_WOOD));
    CHECK(cave_environment_bridge_progress(10,10)==1);
    setcell(10,10,FEAT_LAVA);CHECK(cave_environment_bridge_progress(10,10)==0);
    puts("Bridges: occupied reinforcement, collapse warning, stone replacement, eroded bank rejection, stale-work clearing PASS.");
}
static void test_actors(void)
{
    clean();setcell(10,10,FEAT_BRIDGE_WATER_H);cave_environment_seed();
    cave_environment_flood_bridge(10,10,FEAT_WATER,5);
    CHECK(place_monster_one(10,9,41,false,false,NULL));
    monster_type* m=&mon_list[cave_m_idx[10][9]];
    m->alertness=ALERTNESS_UNWARY-5;m->target_y=0;m->target_x=0;
    cave_event_emit(CAVE_EVENT_COLLAPSE,10,11,30);
    for(int n=0;n<4;n++){m->energy=100;process_monsters(0);}
    CHECK(m->world.initialized&&m->world.supplies==20);
    CHECK(cave_environment_cell_at(10,10)->integrity==100);
    CHECK(m->ai.sense.kind==MON_SENSE_NONE);
    CHECK(!m->target_y&&!m->target_x);
    CHECK(m->alertness>=ALERTNESS_UNWARY&&m->alertness<ALERTNESS_ALERT);
    /* Real obstacle propagation cannot leak an event through a sealed wall. */
    for(int y=1;y<19;y++)setcell(y,12,FEAT_WALL_PERM);
    cave_events_reset();cave_event_emit(CAVE_EVENT_COLLAPSE,10,14,64);
    cave_world_event event;
    CHECK(!cave_event_for_listener(10,9,10,0,&event));
    clean();setcell(10,10,FEAT_CHASM);cave_environment_seed();
    CHECK(place_monster_one(10,9,41,false,false,NULL));m=&mon_list[cave_m_idx[10][9]];
    m->alertness=ALERTNESS_UNWARY;m->target_y=m->target_x=0;
    cave_event_emit(CAVE_EVENT_COLLAPSE,10,13,30);
    for(int n=0;n<8;n++){m->energy=100;process_monsters(0);}
    CHECK(FEAT_IS_BRIDGE(cave_feat[10][10])&&m->world.supplies==16);
    CHECK(m->ai.sense.kind==MON_SENSE_NONE);
    cave_environment_flood_bridge(10,10,FEAT_CHASM,2);
    monster_senses_hear(m,p_ptr->py,p_ptr->px);m->alertness=ALERTNESS_ALERT;
    int supplies=m->world.supplies;CHECK(!monster_world_turn(m));CHECK(m->world.supplies==supplies);
    environment_cell warning=*cave_environment_cell_at(m->fy,m->fx);
    warning.pending_feat=FEAT_CHASM;warning.due=turn+50;
    CHECK(cave_environment_restore_cell(m->fy,m->fx,warning));
    int old_y=m->fy,old_x=m->fx;
    CHECK(monster_world_turn(m));CHECK(m->fy!=old_y||m->fx!=old_x);
    clean();cave_environment_seed();
    CHECK(place_monster_one(10,9,31,false,false,NULL));m=&mon_list[cave_m_idx[10][9]];
    m->alertness=ALERTNESS_UNWARY;
    cave_event_emit(CAVE_EVENT_BUILD,10,14,20);monster_world_observe(m);
    CHECK(m->world.observation_kind==CAVE_EVENT_BUILD);
    CHECK(monster_world_turn(m)&&m->world.task==MON_WORLD_INVESTIGATE);
    CHECK(m->ai.sense.kind==MON_SENSE_NONE);
    puts("Actors: real offscreen scheduler wakes to unwary and repairs without player memory; wall blocks physical clue PASS.");
}
static void test_sole_route(void)
{
    clean();p_ptr->py=10;p_ptr->px=2;
    for(int y=1;y<19;y++)for(int x=1;x<23;x++)setcell(y,x,FEAT_WALL_PERM);
    for(int x=2;x<=20;x++)setcell(10,x,FEAT_FLOOR);
    setcell(10,10,FEAT_BRIDGE_CHASM_H);setcell(10,20,FEAT_MORE);
    cave_environment_seed();cave_environment_flood_bridge(10,10,FEAT_CHASM,100);
    CHECK(!cave_environment_pending_hazard(10,10));
    tick(400);CHECK(cave_feat[10][10]==FEAT_BRIDGE_CHASM_H);
    puts("Connectivity: an actively failing sole stair crossing cannot collapse PASS.");
}
static void test_protection(void)
{
    clean();setcell(2,20,FEAT_MORE);setcell(4,20,FEAT_LESS);
    setcell(10,10,FEAT_WATER);setcell(11,10,FEAT_QUARTZ);
    for(int y=8;y<15;y++)for(int x=8;x<16;x++) {
        cave_natural[y][x]=1;
        if(x>=12)setcell(y,x,FEAT_WALL_EXTRA);
    }
    setcell(10,11,FEAT_FLOOR);cave_info[10][11]|=CAVE_G_VAULT;
    for(int y=14;y<=16;y++)for(int x=17;x<=19;x++){
        setcell(y,x,FEAT_WALL_EXTRA);cave_natural[y][x]=1;
    }
    setcell(15,18,FEAT_QUARTZ);
    cave_environment_seed();
    int last_budget=8;
    for(int n=0;n<12000;n++) {
        tick(1);
        int budget=cave_environment_get_state().mineral_budget;
        CHECK(budget>=0&&budget<=last_budget);last_budget=budget;
        CHECK(cave_feat[2][20]==FEAT_MORE&&cave_feat[4][20]==FEAT_LESS);
        CHECK(cave_feat[10][11]==FEAT_FLOOR);
    }
    static byte reached[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    terrain_generation_flood(2,2,reached,NULL);
    CHECK(reached[2][20]&&reached[4][20]);
    CHECK(last_budget<8);
    puts("Long run: finite mineral budget, stairs/vault protection and connected stair routes PASS.");
}
'''

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix=ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]
    for header in ('sdl-config.h','ui/question.h','monster/monster-ai.h','monster/monster-senses.h','monster/monster-world.h',
                   'cave/cave-fixtures.h','cave/cave-flood.h','cave/cave-water-flow.h',
                   'cave/cave-environment.h','cave/cave-events.h','level-generation/level-generation-terrain-access.h'):
        prefix+=f'\n#include "{header}"\n'
    init=ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init=init[:init.index('    check_templates();')]
    source=OUT/'check.c'
    source.write_text(prefix+fixture_function('terminal_extra')+'\n'+fixture_function('reset_map')+'\n'+FRESH_MAP+'\n'+TESTS+'\n'+init+
        '    test_environment_speed();test_environment_speed_settings();test_clock();test_reservoir();test_thermal();test_utumno_contacts();test_floor_chasm_bridges();test_partition_heat();test_bridges();test_actors();test_protection();test_sole_route();test_memory();test_vents();test_debug_options();test_wizard_event_travel();\n'
        '    printf("Living dungeon behavior: %d checks, %d failed groups.\\n",checks,failures);\n'
        '    SDL_Quit();return failures?1:0;\n}\n',encoding='utf-8')
    objects=shlex.split((BUILD/'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    objects=[p for p in objects if not p.endswith(('/src/main.c.obj','/src/cmd/ui/cmd-ui-settings.c.obj'))]
    settings=OUT/'settings-check.c'
    settings.write_text('#include "cmd/ui/cmd-ui-settings.c"\n'
        'bool test_pick_environment_speed(bool* handled) { return option_pick_value(OPT_environment_speed,handled); }\n'
        'void test_reset_environment_speed(bool* dirty) { const int opt[]={OPT_environment_speed}; bool sound=false,meta=false; options_aux_reset_to_default(GAMEPLAY_PAGE,opt,0,true,NULL,dirty,&sound,&meta); }\n',encoding='utf-8')
    response=OUT/'objects.rsp';response.write_text('\n'.join('"'+p+'"' for p in objects),encoding='utf-8')
    env=os.environ.copy()
    env['PATH']=os.pathsep.join([*(str(BUILD/'_deps'/name) for name in ('SDL','SDL_ttf','SDL_image','SDL_mixer')),
        'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    exe=OUT/'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0','-g',
        '@CMakeFiles/sil-more.dir/includes_C.rsp',str(source),str(settings),'@'+str(response),
        '@CMakeFiles/sil-more.dir/linkLibs.rsp','-Wl,--wrap=ui_question_ask',
        '-Wl,--wrap=ui_question_ask_overlay_buttons','-o',str(exe)],cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix='data-',dir=OUT) as data:
        subprocess.run([str(exe),str(ROOT/'lib/edit'),data],cwd=data,env=env,check=True,timeout=120)

if __name__=='__main__':main()

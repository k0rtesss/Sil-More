#!/usr/bin/env python3
"""Morgoth's Wrath tests linked to the production engine; never open player saves."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
import sys
from check_living_dungeon_save import ENGINE_FIXTURE, fixture_function, FRESH_MAP, WRITER, READER

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build-standard'
OUT = ROOT / 'scripts/output/catastrophe'

TESTS = r'''
size_t fixture_write_dungeon(byte*,size_t,size_t*);
int fixture_read_dungeon(const byte*,size_t,int,u32b*,size_t*);
static int checks;
#define CHECK(t) do { checks++; if(!(t)){fprintf(stderr,"FAIL %s:%d: %s\n",__func__,__LINE__,#t);exit(1);} } while(0)
static int debug_pick=-1,debug_calls,cold_calls;
void __real_cold_dam_pure(int,int,bool,cptr);
void __wrap_cold_dam_pure(int dd,int ds,bool rolls,cptr killer)
{
    cold_calls++;__real_cold_dam_pure(dd,ds,rolls,killer);
}
static char debug_command='!';
static bool debug_real_hotkey;
int __real_ui_question_ask(cptr,cptr,const ui_question_option*,int,int,int,int);
static int acquisition_difficulty=-1;
int __real_object_intrinsic_difficulty(const object_type*);
int __wrap_object_intrinsic_difficulty(const object_type* obj)
{
    return acquisition_difficulty>=0?acquisition_difficulty:__real_object_intrinsic_difficulty(obj);
}
int __wrap_ui_question_ask(cptr title,cptr desc,const ui_question_option* options,
    int count,int y,int x,int initial)
{
    (void)desc;(void)y;(void)x;(void)initial;debug_calls++;
    if(streq(title,"Trigger catastrophe")) {
        CHECK(count==CATA_KIND_MAX);CHECK(options[CATA_LAVA].disabled);
        return debug_pick;
    }
    if(streq(title,"Map and Travel")) {
        /* This menu's real input matcher folds case: W must not shadow w. */
        for(int i=0;i<count;i++)for(int j=i+1;j<count;j++)
            if(options[i].key&&options[j].key)
                CHECK(tolower((unsigned char)options[i].key)!=tolower((unsigned char)options[j].key));
        if(debug_real_hotkey) {
            message_flush();inkey_xtra=false;Term_flush();Term_keypress(debug_command);
            int choice=__real_ui_question_ask(title,desc,options,count,y,x,initial);
            CHECK(choice>=0 && options[choice].key==debug_command);
            return choice;
        }
        for(int i=0;i<count;i++)if(options[i].key==debug_command)return i;
    }
    for(int i=0;i<count;i++)if(strstr(options[i].label,"Map and Travel"))return i;
    return -1;
}
static void clean(void)
{
    fresh_map();catastrophe_reset_run();turn=1000;playerturn=100;
    p_ptr->game_type=0;p_ptr->depth=10;p_ptr->energy_use=100;
    p_ptr->py=2;p_ptr->px=2;p_ptr->chp=p_ptr->mhp=30000;
    p_ptr->resist_fire=20;p_ptr->resist_cold=20;p_ptr->resist_pois=20;
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    player_quiver_reset_store();acquisition_difficulty=-1;
    debug_command='!';debug_real_hotkey=false;cold_calls=0;
    partition_meta_save pm={0};pm.grid_rows=pm.grid_cols=pm.partition_count=1;
    pm.modes[0]=QUAD_MODE_ROOMY;level_partition_meta_set(&pm);
    character_dungeon=true;cheat_timestop=false;
    cave_feat[2][2]=FEAT_MORE;cave_m_idx[2][2]=-1;
    cave_environment_seed();
}
static void action(void)
{
    playerturn++;turn+=10;p_ptr->energy_use=100;
    catastrophe_begin_action();player_lava_begin_action();player_poison_terrain_begin_action();
    catastrophe_end_action();player_lava_end_action();player_poison_terrain_end_action();
}
static int count_feat(int feat)
{
    int n=0;for(int y=1;y<p_ptr->cur_map_hgt-1;y++)for(int x=1;x<p_ptr->cur_map_wid-1;x++)
        n+=cave_feat[y][x]==feat||(feat==FEAT_WATER&&cave_feat[y][x]==FEAT_DEEP_WATER);
    return n;
}
static u32b fail_seed(int count)
{
    for(u32b seed=1;;seed++) {
        u32b r=seed;bool good=true;
        for(int i=0;i<count;i++){r^=r<<13;r^=r>>17;r^=r<<5;if(r%100<90)good=false;}
        if(good)return seed;
    }
}
static void setchance(int chance,int failures)
{
    catastrophe_state s=catastrophe_get_state();s.chance=chance;s.random=fail_seed(failures);
    CHECK(catastrophe_restore_state(s));
}
static void test_probability(void)
{
    const int increments[]={5,7,5,3,3,1,1,2};
    for(int e=0;e<CATA_EVENT_MAX;e++) {
        clean();setchance(30,1);catastrophe_note(e);catastrophe_flush_events();
        CHECK(catastrophe_chance()==30+increments[e]);CHECK(!catastrophe_active());
    }
    clean();setchance(42,1);p_ptr->depth=2;CHECK(catastrophe_chance()==42);
    p_ptr->depth=50;CHECK(catastrophe_chance()==50);
    clean();cave_set_feat(10,10,FEAT_WATER);setchance(100,1);
    catastrophe_note(CATA_UNIQUE);catastrophe_flush_events();CHECK(catastrophe_active());
    CHECK(catastrophe_get_state().chance==10);
    unsigned step=catastrophe_get_state().step;
    catastrophe_note(CATA_VALAR);catastrophe_flush_events();CHECK(catastrophe_get_state().step==step);
    CHECK(catastrophe_chance()==100);
    clean();int threshold;min_depth_timer_status(NULL,NULL,NULL,NULL,&threshold);
    catastrophe_state s=catastrophe_get_state();s.last_stage=8;s.timer_step=threshold;s.random=fail_seed(2);
    CHECK(catastrophe_restore_state(s));catastrophe_depth_tick(9);CHECK(!catastrophe_get_state().pending[CATA_TIMER]);
    catastrophe_depth_tick(10);CHECK(catastrophe_get_state().pending[CATA_TIMER]==1);
    bool generated=character_generated;character_generated=true;
    catastrophe_flush_events();character_generated=generated;CHECK(catastrophe_chance()==17);
    CHECK(strstr(message_str(0),"10%")&&strstr(message_str(0),"17%"));
    catastrophe_depth_tick(10);CHECK(!catastrophe_get_state().pending[CATA_TIMER]);
    p_ptr->depth=20;catastrophe_sync_depth(20);catastrophe_depth_tick(21);
    CHECK(catastrophe_get_state().pending[CATA_TIMER]==1);
    puts("Exact event increments, depth ratchet, certainty, active suppression and uncapped timer: PASS");
}
static object_type artifact(int id)
{
    object_type obj={0};object_prep(&obj,lookup_kind(a_info[id].tval,a_info[id].sval));
    obj.name1=id;obj.number=1;return obj;
}
static void test_milestones(void)
{
    const int boundaries[]={14,15,24,25,34,35,44,45,54,55};
    object_type obj=artifact(81);
    for(int i=0;i<10;i++) {
        clean();catastrophe_accept_craft(boundaries[i]);catastrophe_crafted(&obj);
        unsigned n=boundaries[i]<15?0:MIN(5,(boundaries[i]-15)/10+1);
        catastrophe_state s=catastrophe_get_state();
        CHECK(s.smith_milestones==(1U<<n)-1);CHECK(s.pending[CATA_SMITH]==n);
        CHECK(!s.find_milestones);
        clean();acquisition_difficulty=boundaries[i];catastrophe_acquired(&obj);
        s=catastrophe_get_state();CHECK(s.find_milestones==(1U<<n)-1);
        CHECK(s.pending[CATA_FIND]==n && !s.smith_milestones);
    }
    clean();catastrophe_accept_craft(37);catastrophe_crafted(&obj);
    CHECK(catastrophe_get_state().smith_milestones==7);CHECK(catastrophe_get_state().pending[CATA_SMITH]==3);
    setchance(10,3);catastrophe_flush_events();CHECK(catastrophe_chance()==19);
    catastrophe_accept_craft(48);catastrophe_crafted(&obj);
    CHECK(catastrophe_get_state().smith_milestones==15);CHECK(catastrophe_get_state().pending[CATA_SMITH]==1);
    clean();object_type plain=obj;plain.name1=0;catastrophe_accept_craft(60);catastrophe_crafted(&plain);
    CHECK(!catastrophe_get_state().smith_milestones);
    clean();catastrophe_acquired(&obj);int count=catastrophe_acquired_count();
    catastrophe_state before=catastrophe_get_state();catastrophe_acquired(&obj);
    CHECK(catastrophe_acquired_count()==count);
    CHECK(catastrophe_get_state().pending[CATA_FIND]==before.pending[CATA_FIND]);
    CHECK(!catastrophe_get_state().smith_milestones);
    object_type crown=artifact(ART_MORGOTH_3);catastrophe_acquired(&crown);count=catastrophe_acquired_count();
    crown.name1=ART_MORGOTH_0;catastrophe_acquired(&crown);CHECK(catastrophe_acquired_count()==count);
    clean();catastrophe_accept_craft(55);catastrophe_crafted(&obj);catastrophe_acquired(&obj);
    CHECK(!catastrophe_get_state().pending[CATA_FIND]);
    /* Production possession hooks reject failed absorption and deduplicate
     * successful mixed-quiver transfers, including a legacy baseline. */
    clean();object_type incoming=obj;CHECK(!player_quiver_absorb_arrow(&incoming));
    CHECK(!catastrophe_acquired_count());
    int arrow=0;for(int i=1;i<z_info->art_rand_max;i++)if(a_info[i].tval==TV_ARROW){arrow=i;break;}
    CHECK(arrow>0);incoming=artifact(arrow);CHECK(player_quiver_absorb_arrow(&incoming)==1);
    CHECK(catastrophe_acquired_count()==1);before=catastrophe_get_state();
    catastrophe_scan_inventory(false);CHECK(catastrophe_acquired_count()==1);
    CHECK(catastrophe_get_state().pending[CATA_FIND]==before.pending[CATA_FIND]);
    catastrophe_reset_run();catastrophe_scan_inventory(true);catastrophe_scan_inventory(false);
    CHECK(catastrophe_acquired_count()==1&&!catastrophe_get_state().pending[CATA_FIND]);
    const int skills[]={S_MEL,S_ARC,S_EVN,S_STL,S_PER,S_WIL,S_SMT,S_SNG};
    const int abilities[]={MEL_STR,ARC_DEX,EVN_DEX,STL_DEX,PER_GRA,WIL_CON,SMT_GRA,SNG_GRA};
    clean();for(int i=0;i<8;i++)catastrophe_ability(skills[i],abilities[i]);
    CHECK(catastrophe_get_state().pending[CATA_ABILITY]==8);
    catastrophe_ability(S_MEL,MEL_POWER);CHECK(catastrophe_get_state().pending[CATA_ABILITY]==8);
    puts("Artefact thresholds, multiple rolls, independent masks, identity, Crown forms and attribute entries: PASS");
}
static void corridor(int source)
{
    clean();
    for(int y=1;y<19;y++)for(int x=1;x<23;x++)cave_set_feat(y,x,FEAT_WALL_PERM);
    cave_set_feat(2,2,FEAT_MORE);
    for(int x=2;x<22;x++)cave_set_feat(10,x,FEAT_FLOOR);
    cave_set_feat(10,2,source);cave_environment_seed();
}
static void test_spread(void)
{
    clean();cave_set_feat(10,10,FEAT_WATER);CHECK(catastrophe_start(CATA_WATER));
    CHECK(count_feat(FEAT_WATER)==9);CHECK(catastrophe_get_state().step==1);
    catastrophe_end_action();CHECK(count_feat(FEAT_WATER)==9);
    catastrophe_begin_action();p_ptr->energy_use=0;catastrophe_end_action();CHECK(count_feat(FEAT_WATER)==9);
    action();CHECK(count_feat(FEAT_WATER)==25);action();CHECK(count_feat(FEAT_WATER)==49);
    for(int i=0;i<4;i++)action();CHECK(cave_feat[10][10]==FEAT_DEEP_WATER);
    CHECK(cave_feat[2][2]==FEAT_MORE);
    clean();p_ptr->morgoth_state=6;cave_set_feat(10,10,FEAT_WATER);CHECK(catastrophe_start(CATA_WATER));
    CHECK(count_feat(FEAT_WATER)==25);
    corridor(FEAT_WATER);cave_set_feat(10,4,FEAT_DOOR_HEAD);cave_environment_seed();
    CHECK(catastrophe_start(CATA_WATER));CHECK(cave_feat[10][4]==FEAT_DOOR_HEAD);
    action();CHECK(cave_feat[10][4]==FEAT_WATER);
    CHECK(cave_feat[9][4]==FEAT_WALL_PERM);
    corridor(FEAT_WATER);cave_set_feat(10,4,FEAT_WALL_EXTRA);cave_environment_seed();
    CHECK(catastrophe_start(CATA_WATER));for(int i=0;i<10;i++)action();
    CHECK(cave_feat[10][4]==FEAT_WALL_EXTRA&&cave_feat[10][5]==FEAT_FLOOR);
    clean();for(int y=1;y<19;y++)for(int x=1;x<23;x++)cave_set_feat(y,x,FEAT_WALL_PERM);
    cave_set_feat(2,2,FEAT_MORE);cave_set_feat(10,10,FEAT_WATER);cave_set_feat(11,11,FEAT_FLOOR);
    cave_environment_seed();CHECK(catastrophe_start(CATA_WATER));for(int i=0;i<5;i++)action();
    CHECK(cave_feat[11][11]==FEAT_FLOOR);
    for(int part=QUAD_MODE_RUINED;part<=QUAD_MODE_LABYRINTH;part++) {
        clean();partition_meta_save pm={0};pm.grid_rows=pm.grid_cols=pm.partition_count=1;pm.modes[0]=part;
        level_partition_meta_set(&pm);CHECK(!catastrophe_can_start(CATA_AUTO));
        cave_set_feat(10,10,FEAT_POISON);cave_environment_seed();CHECK(catastrophe_can_start(CATA_ACID));
        CHECK(catastrophe_start(CATA_ACID));CHECK(count_feat(FEAT_POISON)==9);
    }
    puts("Immediate onset, bounded travel, action guards, supply-limited deepening, anger, doors and corners: PASS");
}
static void test_force(void)
{
    clean();cave_set_feat(10,10,FEAT_BRIDGE_WATER_H);cave_environment_seed();
    environment_cell channel=*cave_environment_cell_at(10,10);
    channel.underlay=FEAT_POISON;CHECK(cave_environment_restore_cell(10,10,channel));
    CHECK(catastrophe_can_start(CATA_ACID));CHECK(!catastrophe_can_start(CATA_WATER));
    CHECK(catastrophe_start(CATA_ACID));for(int i=0;i<4;i++)action();CHECK(cave_feat[10][10]==FEAT_POISON);
    int acid_turns=0,lava_turns=0;
    for(int kind=CATA_ACID;kind<=CATA_LAVA;kind++) {
        int feature=kind==CATA_ACID?FEAT_POISON:FEAT_LAVA;
        corridor(feature);cave_set_feat(10,3,FEAT_WALL_EXTRA);cave_environment_seed();
        CHECK(catastrophe_start(kind));int t=1;
        CHECK(cave_environment_cell_at(10,3)->integrity==(kind==CATA_ACID?95:85));
        while(cave_feat[10][4]==FEAT_FLOOR&&t<30){action();t++;}
        CHECK(t<30);if(kind==CATA_ACID)acid_turns=t;else lava_turns=t;
        CHECK(cave_feat[9][3]==FEAT_WALL_PERM);
    }
    CHECK(lava_turns<acid_turns);
    for(int kind=CATA_WATER;kind<=CATA_LAVA;kind++)for(int stone=0;stone<2;stone++) {
        int feature=kind==CATA_WATER?FEAT_WATER:kind==CATA_ACID?FEAT_POISON:FEAT_LAVA;
        corridor(feature);int bridge=stone?FEAT_BRIDGE_CHASM_H:FEAT_BRIDGE_WATER_H;
        cave_set_feat(10,4,bridge);cave_environment_seed();
        /* Isolate deck destruction from water/lava quenching, tested below. */
        environment_cell deck=*cave_environment_cell_at(10,4);deck.underlay=FEAT_CHASM;
        CHECK(cave_environment_restore_cell(10,4,deck));CHECK(catastrophe_start(kind));
        for(int i=0;i<24;i++)action();
        CHECK(kind==CATA_WATER&&stone?cave_feat[10][4]==bridge:!FEAT_IS_BRIDGE(cave_feat[10][4]));
    }
    printf("Wall breach: acid %d actions, lava %d actions; bridge material differences: PASS\n",acid_turns,lava_turns);
}
static void test_escape(void)
{
    clean();CHECK(!catastrophe_can_start(CATA_AUTO));p_ptr->on_the_run=true;
    catastrophe_prepare_level();cave_environment_seed();catastrophe_resume_level();
    CHECK(catastrophe_active());unsigned step=catastrophe_get_state().step;
    catastrophe_resume_level();CHECK(catastrophe_get_state().step==step);
    p_ptr->depth=9;catastrophe_prepare_level();cave_environment_seed();catastrophe_resume_level();
    CHECK(catastrophe_active()&&catastrophe_get_state().step==1);
    p_ptr->depth=0;catastrophe_prepare_level();catastrophe_resume_level();CHECK(!catastrophe_active());
    puts("Source-free escape initialization, resume without extra step, next level and surface: PASS");
}
static void test_occupants_and_debug(void)
{
    /* Test real cold resistance and immediate occupied-tile conversions. */
    corridor(FEAT_ICE);cave_m_idx[2][2]=0;p_ptr->py=10;p_ptr->px=3;
    cave_m_idx[10][3]=-1;p_ptr->resist_cold=5;
    CHECK(catastrophe_start(CATA_ICE));CHECK(cave_feat[10][3]==FEAT_ICE);
    p_ptr->chp=p_ptr->mhp=1000;p_ptr->resist_cold=5;
    action();int resisted=1000-p_ptr->chp;CHECK(resisted>0&&resisted<=8);
    CHECK(!cave_environment_pending_hazard(10,3));
    /* A monster cannot hold back the flood by standing in its way. */
    corridor(FEAT_POISON);CHECK(place_monster_one(10,3,41,false,false,NULL));
    int idx=cave_m_idx[10][3];CHECK(catastrophe_start(CATA_ACID));
    CHECK(cave_feat[10][3]==FEAT_POISON&&mon_list[idx].poisoned>0);
    /* The sole stair approach is allowed to disappear. */
    corridor(FEAT_CHASM);cave_set_feat(10,8,FEAT_MORE);cave_environment_seed();
    CHECK(catastrophe_start(CATA_CHASM));for(int i=0;i<12;i++)action();
    CHECK(cave_feat[10][7]==FEAT_CHASM&&cave_feat[10][8]==FEAT_MORE);
    clean();cave_set_feat(10,10,FEAT_WATER);debug_pick=-1;debug_calls=0;
    debug_real_hotkey=true;do_cmd_debug();CHECK(!catastrophe_active());
    debug_pick=CATA_WATER;do_cmd_debug();CHECK(catastrophe_active());
    unsigned step=catastrophe_get_state().step;do_cmd_debug();
    CHECK(catastrophe_get_state().step==step);
    char description[512];character_sheet_format_vital_description("Catastrophe",description,sizeof(description));
    CHECK(strstr(description,"100%")&&strstr(description,"active"));
    puts("Occupied ice/acid, cold resistance, lost stair approach, debug cancel/start/repeat and sheet description: PASS");
}
static void test_front_events_and_reveal(void)
{
    clean();cave_set_feat(10,10,FEAT_WATER);cave_events_reset();
    CHECK(catastrophe_start(CATA_WATER));
    cave_world_event first,next;CHECK(cave_event_latest(&first));
    CHECK(first.kind==CAVE_EVENT_FLOOD && (first.y!=10||first.x!=10));
    CHECK(cave_feat[first.y][first.x]==FEAT_WATER);
    byte before[20][24];for(int y=0;y<20;y++)for(int x=0;x<24;x++)before[y][x]=cave_feat[y][x];
    action();CHECK(cave_event_latest(&next)&&next.serial>first.serial);
    CHECK(next.kind==CAVE_EVENT_FLOOD && before[next.y][next.x]==FEAT_FLOOR);
    CHECK(cave_feat[next.y][next.x]==FEAT_WATER);
    for(int n=0;n<70;n++)action();
    u32b serial=cave_events_next_serial();
    for(int n=0;n<10;n++)action();
    CHECK(cave_events_next_serial()==serial); /* Full basin is silent. */
    corridor(FEAT_WATER);cave_set_feat(10,3,FEAT_WALL_PERM);cave_events_reset();
    CHECK(catastrophe_start(CATA_WATER));CHECK(!cave_event_latest(&next));
    for(int n=0;n<6;n++)action();
    CHECK(cave_event_latest(&next)&&next.y==10&&next.x==2);
    CHECK(cave_feat[next.y][next.x]==FEAT_DEEP_WATER); /* A real deepening event. */
    serial=cave_events_next_serial();
    for(int n=0;n<10;n++)action();
    CHECK(cave_events_next_serial()==serial);
    cave_set_feat(10,2,FEAT_FLOOR);serial=cave_events_next_serial();action();
    CHECK(cave_events_next_serial()==serial); /* Lost source is also silent. */

    for(int command=0;command<2;command++) {
        clean();cave_set_feat(10,12,FEAT_BRIDGE_WATER_H);
        cave_set_feat(10,16,FEAT_BRIDGE_WATER_H);
        for(int y=13;y<=17;y++)for(int x=4;x<=8;x++) {
            cave_set_feat(y,x,FEAT_WALL_EXTRA);cave_info[y][x]|=CAVE_MARK;
        }
        cave_set_feat(10,14,FEAT_WALL_EXTRA);cave_environment_seed();
        cave_info[10][10]=cave_info[10][12]=cave_info[10][14]=cave_info[10][16]=CAVE_MARK;
        cave_set_feat(10,10,FEAT_DEEP_WATER);
        CHECK(cave_environment_catastrophe_contact(10,12,FEAT_POISON,5,100));
        CHECK(cave_environment_catastrophe_contact(10,16,FEAT_POISON,5,1));
        cave_set_feat(10,14,FEAT_FLOOR);
        CHECK(cave_environment_known_feature(10,10)==FEAT_FLOOR);
        CHECK(cave_environment_known_feature(10,12)==FEAT_BRIDGE_WATER_H);
        CHECK(cave_environment_display_underlay(10,12)==FEAT_WATER);
        CHECK(cave_environment_display_underlay(10,16)==FEAT_WATER);
        CHECK(cave_environment_known_feature(10,14)==FEAT_WALL_EXTRA);
        byte old_a,a;char old_c,c;map_info_terrain(10,10,&old_a,&old_c);
        byte old_map_a,map_a,ta;char old_map_c,map_c,tc;
        map_info(10,10,&old_map_a,&old_map_c,&ta,&tc);
        s32b old_turn=turn;debug_command=command?'d':'w';debug_real_hotkey=true;
        do_cmd_debug();
        CHECK(!catastrophe_active()&&turn==old_turn);
        CHECK(!(cave_info[15][6]&CAVE_MARK)); /* Buried solid-wall interior. */
        CHECK(cave_info[13][6]&CAVE_MARK); /* Exposed wall edge remains mapped. */
        CHECK(cave_environment_known_feature(10,10)==FEAT_DEEP_WATER);
        CHECK(cave_environment_known_feature(10,12)==FEAT_POISON);
        CHECK(cave_environment_display_underlay(10,12)==FEAT_POISON);
        CHECK(cave_environment_known_feature(10,16)==FEAT_BRIDGE_WATER_H);
        CHECK(cave_environment_display_underlay(10,16)==FEAT_POISON);
        CHECK(cave_environment_known_feature(10,14)==FEAT_FLOOR);
        CHECK(!(cave_info[10][10]&CAVE_SEEN));
        CHECK((p_ptr->redraw&PR_MAP)&&(p_ptr->window&PW_OVERHEAD));
        map_info_terrain(10,10,&a,&c);CHECK(a!=old_a||c!=old_c);
        map_info(10,10,&map_a,&map_c,&ta,&tc);
        CHECK(map_a!=old_map_a||map_c!=old_map_c);
        /* Revelation is a fresh snapshot, not permanent omniscience. */
        cave_set_feat(10,10,FEAT_ICE);
        CHECK(cave_environment_known_feature(10,10)==FEAT_DEEP_WATER);
    }
    puts("Moving flood event positions; silent full/blocked/lost fronts; real !/w/d hotkeys refresh unseen terrain, bridges and map renderer: PASS");
}
static void test_real_events(void)
{
    clean();r_info[R_IDX_DURUIN].max_num=1;r_info[R_IDX_DURUIN].cur_num=0;
    CHECK(place_monster_one(10,10,R_IDX_DURUIN,false,true,NULL));
    int idx=cave_m_idx[10][10];mon_list[idx].hp=1;
    CHECK(mon_take_hit(idx,1000,NULL,-1));CHECK(catastrophe_get_state().pending[CATA_UNIQUE]==1);
    clean();r_info[R_IDX_DURUIN].max_num=1;r_info[R_IDX_DURUIN].cur_num=0;
    CHECK(place_monster_one(10,10,R_IDX_DURUIN,false,true,NULL));idx=cave_m_idx[10][10];mon_list[idx].hp=1;
    CHECK(mon_take_hit(idx,1000,NULL,0));CHECK(!catastrophe_get_state().pending[CATA_UNIQUE]);
    for(int unique=0;unique<2;unique++) {
        clean();r_info[R_IDX_DURUIN].max_num=1;r_info[R_IDX_DURUIN].cur_num=0;
        CHECK(place_monster_one(10,10,unique?R_IDX_DURUIN:41,false,true,NULL));
        idx=cave_m_idx[10][10];mon_list[idx].ml=true;mon_list[idx].song_contest_stacks=2;
        p_ptr->song_target_idx=idx;p_ptr->song_target_song=SNG_CONTEST;p_ptr->song1=SNG_CONTEST;
        p_ptr->skill_use[S_WIL]=1000;
        CHECK(!song_duel_process_contest(1000));
        CHECK(catastrophe_get_state().pending[CATA_SONG]==(unsigned)unique);
    }
    puts("Real player/environment unique deaths and completed unique/non-unique song duels: PASS");
}
static void test_reactions(void)
{
    corridor(FEAT_LAVA);cave_set_feat(10,3,FEAT_WATER);
    CHECK(catastrophe_start(CATA_LAVA));
    CHECK(cave_feat[10][2]==FEAT_FLOOR && cave_feat[10][4]==FEAT_FLOOR);
    CHECK(!cave_environment_pending_hazard(10,2));
    action();CHECK(count_feat(FEAT_LAVA)==0);
    corridor(FEAT_WATER);cave_set_feat(10,3,FEAT_LAVA);
    CHECK(catastrophe_start(CATA_WATER));
    CHECK(cave_feat[10][3]==FEAT_WATER && cave_feat[10][4]==FEAT_FLOOR);
    action();CHECK(cave_feat[10][4]==FEAT_WATER);
    corridor(FEAT_LAVA);cave_set_feat(10,3,FEAT_ICE);
    CHECK(catastrophe_start(CATA_LAVA));CHECK(cave_feat[10][3]==FEAT_MELTING_ICE);
    action();CHECK(cave_feat[10][3]==FEAT_WATER);
    action();CHECK(cave_feat[10][2]==FEAT_FLOOR);
    corridor(FEAT_ICE);cave_set_feat(10,3,FEAT_DOOR_HEAD);
    CHECK(catastrophe_start(CATA_ICE));action();
    CHECK(cave_feat[10][3]==FEAT_DOOR_HEAD && cave_feat[10][4]==FEAT_FLOOR);
    for(int i=0;i<3;i++)action();
    CHECK(cave_feat[10][3]==FEAT_DOOR_HEAD && cave_feat[10][4]==FEAT_ICE);
    puts("Immediate quenching, blocked quenched origin, two-stage ice melting and renewed water connectivity: PASS");
}
static void test_cold_structures_and_supply(void)
{
    const int structures[]={FEAT_DOOR_HEAD,FEAT_SECRET,FEAT_WARDED3,FEAT_OPEN,
        FEAT_BROKEN,FEAT_WALL_EXTRA,FEAT_QUARTZ,FEAT_WALL_PERM,
        FEAT_BRIDGE_WATER_H,FEAT_BRIDGE_CHASM_H,FEAT_TRAP_PIT,FEAT_FORGE};
    for(unsigned i=0;i<sizeof(structures)/sizeof(*structures);i++) {
        corridor(FEAT_ICE);cave_set_feat(10,3,structures[i]);cave_environment_seed();
        CHECK(catastrophe_start(CATA_ICE));
        CHECK(catastrophe_owns(10,3));CHECK(cave_feat[10][3]==structures[i]);
        for(int n=0;n<9;n++)action();
        CHECK(cave_feat[10][3]==structures[i]);CHECK(cave_feat[10][4]==FEAT_ICE);
        CHECK(cave_feat[0][0]==FEAT_WALL_PERM);CHECK(!catastrophe_owns(0,0));
    }
    /* Bulk water needs cooling beyond the thin ground coating. */
    corridor(FEAT_ICE);cave_set_feat(10,3,FEAT_DEEP_WATER);cave_environment_seed();
    CHECK(catastrophe_start(CATA_ICE));CHECK(cave_feat[10][3]==FEAT_DEEP_WATER);
    action();action();CHECK(cave_feat[10][3]==FEAT_DEEP_WATER);
    action();CHECK(cave_feat[10][3]==FEAT_ICE);
    /* Freezing reaches a preserved deck and applies exactly one cold pulse. */
    corridor(FEAT_ICE);cave_set_feat(10,3,FEAT_BRIDGE_WATER_H);cave_environment_seed();
    cave_m_idx[2][2]=0;p_ptr->py=10;p_ptr->px=3;cave_m_idx[10][3]=-1;
    p_ptr->resist_cold=1;p_ptr->chp=p_ptr->mhp=1000;
    CHECK(catastrophe_start(CATA_ICE));CHECK(cave_feat[10][3]==FEAT_BRIDGE_WATER_H);
    CHECK(cold_calls==1);int hp=p_ptr->chp;action();
    CHECK(cold_calls==2 && hp-p_ptr->chp>=2 && hp-p_ptr->chp<=8);
    /* Two high-anger travel substeps must not apply deck contact twice. */
    corridor(FEAT_WATER);p_ptr->morgoth_state=6;cave_set_feat(10,3,FEAT_BRIDGE_WATER_H);
    cave_environment_seed();CHECK(catastrophe_start(CATA_WATER));
    CHECK(cave_environment_cell_at(10,3)->integrity==4);
    action();CHECK(!FEAT_IS_BRIDGE(cave_feat[10][3]));
    /* Deepening cannot create an unlimited second supply behind the front. */
    for(int a=0;a<=6;a+=2) {
        clean();p_ptr->morgoth_state=a;cave_set_feat(10,10,FEAT_WATER);
        CHECK(catastrophe_start(CATA_WATER));
        for(int n=0;n<40;n++) {
            int old_water=count_feat(FEAT_WATER),old_deep=count_feat(FEAT_DEEP_WATER);
            action();int used=count_feat(FEAT_WATER)-old_water+count_feat(FEAT_DEEP_WATER)-old_deep;
            CHECK(used>=0 && used<=32+8*a);
        }
    }
    puts("Cold crosses intact structures, water cooling, cold on bridges, single contact and finite shared water supply: PASS");
}

static void test_disconnected_front(void)
{
    const int features[]={0,FEAT_WATER,FEAT_POISON,FEAT_LAVA,FEAT_ICE,FEAT_CHASM};
    for(int kind=CATA_WATER;kind<CATA_KIND_MAX;kind++) {
        if(kind==CATA_ICE)continue; /* Cold deliberately crosses the barrier. */
        corridor(features[kind]);p_ptr->morgoth_state=6;
        CHECK(catastrophe_start(kind));
        for(int n=0;n<15 && cave_feat[10][8]==FEAT_FLOOR;n++)action();
        CHECK(cave_feat[10][8]!=FEAT_FLOOR && cave_feat[10][9]==FEAT_FLOOR);
        cave_set_feat(10,5,FEAT_WALL_PERM);
        int stranded=cave_feat[10][8];
        for(int n=0;n<8;n++)action();
        CHECK(cave_feat[10][8]==stranded && cave_feat[10][9]==FEAT_FLOOR);
        cave_set_feat(10,5,FEAT_FLOOR);action();action();
        CHECK(cave_feat[10][9]==features[kind]);
    }
    puts("Disconnected liquid/chasm fronts stop, cannot deepen remotely, and resume after physical reconnection: PASS");
}

static void test_material_save_continuation(void)
{
    static byte encoded[1048576],expected[1048576],actual[1048576];
    const int features[]={0,FEAT_WATER,FEAT_POISON,FEAT_LAVA,FEAT_ICE,FEAT_CHASM};
    for(int kind=CATA_WATER;kind<CATA_KIND_MAX;kind++) {
        clean();p_ptr->morgoth_state=2;cave_set_feat(10,10,features[kind]);
        cave_set_feat(10,11,FEAT_DOOR_HEAD);cave_set_feat(10,12,FEAT_WALL_EXTRA);
        cave_set_feat(11,12,FEAT_WALL_PERM);cave_environment_seed();catastrophe_resume_level();
        CHECK(catastrophe_start(kind));action();action();
        size_t end=0,length=fixture_write_dungeon(encoded,sizeof(encoded),&end),consumed=0;
        u32b sentinel=0;catastrophe_state saved=catastrophe_get_state();
        s32b saved_turn=turn,saved_playerturn=playerturn;
        for(int i=0;i<12;i++)action();
        size_t expected_length=fixture_write_dungeon(expected,sizeof(expected),&end);
        catastrophe_reset_run();
        CHECK(fixture_read_dungeon(encoded,length,VERSION_EXTRA,&sentinel,&consumed)==0);
        CHECK(consumed==length && sentinel==0xA1B2C3D4U);
        CHECK(catastrophe_get_state().step==saved.step);
        turn=saved_turn;playerturn=saved_playerturn;
        catastrophe_resume_level();CHECK(catastrophe_get_state().step==saved.step);
        for(int i=0;i<12;i++)action();
        size_t actual_length=fixture_write_dungeon(actual,sizeof(actual),&end);
        if(actual_length!=expected_length || memcmp(actual,expected,MIN(actual_length,expected_length))) {
            fprintf(stderr,"Continuation kind=%d lengths=%zu/%zu\n",kind,actual_length,expected_length);
            for(size_t i=0;i<MIN(actual_length,expected_length);i++) {
                int av=actual[i]^(i?actual[i-1]:0),ev=expected[i]^(i?expected[i-1]:0);
                if(av!=ev){fprintf(stderr,"First decoded mismatch at %zu: %d/%d\n",i,av,ev);break;}
            }
        }
        CHECK(actual_length==expected_length && !memcmp(actual,expected,actual_length));
    }
    puts("All five materials: exact serialized continuation across a mid-front save/load, including cold in walls: PASS");
}

static void test_sheets(void)
{
    const int widths[]={80,60,40};
    for(int active=0;active<2;active++) {
        clean();setchance(42,1);
        if(active){cave_set_feat(10,10,FEAT_WATER);CHECK(catastrophe_start(CATA_WATER));}
        sdl_char_sheet_line lines[80];
        int count=sdl_char_sheet_collect_vitals(lines,80),timer=-1,cat=-1;
        for(int i=0;i<count;i++) {
            if(strstr(lines[i].text,"Depth timer"))timer=i;
            if(strstr(lines[i].text,"Catastrophe"))cat=i;
        }
        CHECK(timer>=0 && cat==timer+1);
        CHECK(strstr(lines[cat].text,active?"100% - active":"42%"));
        for(int size=0;size<3;size++) {
            CHECK(Term_resize(widths[size],40)==0);display_player(0);
            char screen[40][81];int found=-1,column=-1;
            for(int y=0;y<40;y++) {
                for(int x=0;x<widths[size];x++) {
                    byte attr;Term_what(x,y,&attr,&screen[y][x]);
                }
                screen[y][widths[size]]=0;
                char* label=strstr(screen[y],"Catastrophe:");
                if(label){found=y;column=(int)(label-screen[y]);}
            }
            CHECK(found>0 && column>=0);
            CHECK(strstr(screen[found],active?"100%":"42%"));
            CHECK(strchr(screen[found-1]+column,'['));
            if(active)CHECK(strstr(screen[found],"active")||strstr(screen[found+1],"active"));
        }
    }
    CHECK(Term_resize(80,24)==0);
    puts("Live SDL vital rows and 80/60/40-column character sheets: probability immediately below timer, active state visible: PASS");
}
static void test_save(void)
{
    static byte encoded[1048576],plain[1048576],legacy[1048576];
    clean();cave_set_feat(10,10,FEAT_WATER);catastrophe_accept_craft(37);
    object_type obj=artifact(81);catastrophe_acquired(&obj);
    CHECK(catastrophe_start(CATA_WATER));action();
    catastrophe_state before=catastrophe_get_state();int identities=catastrophe_acquired_count();
    u32b cells[20][24];for(int y=0;y<20;y++)for(int x=0;x<24;x++)cells[y][x]=catastrophe_cell_step(y,x);
    size_t end=0,length=fixture_write_dungeon(encoded,sizeof(encoded),&end),consumed=0;
    action();catastrophe_state expected=catastrophe_get_state();
    byte next_map[20][24];u32b next_cells[20][24];
    for(int y=0;y<20;y++)for(int x=0;x<24;x++){
        next_map[y][x]=cave_feat[y][x];next_cells[y][x]=catastrophe_cell_step(y,x);
    }
    u32b sentinel=0;catastrophe_reset_run();
    CHECK(fixture_read_dungeon(encoded,length,VERSION_EXTRA,&sentinel,&consumed)==0);
    CHECK(sentinel==0xA1B2C3D4U&&consumed==length);
    catastrophe_state after=catastrophe_get_state();
    CHECK(after.chance==before.chance&&after.random==before.random&&after.step==before.step);
    CHECK(after.craft_difficulty==37&&after.find_milestones==before.find_milestones);
    CHECK(after.pending[CATA_FIND]==before.pending[CATA_FIND]&&identities==catastrophe_acquired_count());
    for(int y=0;y<20;y++)for(int x=0;x<24;x++)CHECK(cells[y][x]==catastrophe_cell_step(y,x));
    catastrophe_resume_level();CHECK(catastrophe_get_state().step==before.step);
    action();CHECK(catastrophe_get_state().step==before.step+1);
    CHECK(catastrophe_get_state().random==expected.random);
    for(int y=0;y<20;y++)for(int x=0;x<24;x++){
        CHECK(cave_feat[y][x]==next_map[y][x]);CHECK(catastrophe_cell_step(y,x)==next_cells[y][x]);
    }
    byte last=0;size_t marker=0;
    for(size_t i=0;i<length;i++){plain[i]=encoded[i]^last;last=encoded[i];}
    for(size_t i=0;i+1<end;i++)if(plain[i]==0x23&&plain[i+1]==0xCA)marker=i;
    CHECK(marker>0);
    memcpy(plain+marker,plain+end,length-end);size_t old_length=marker+length-end;last=0;
    for(size_t i=0;i<old_length;i++){last^=plain[i];legacy[i]=last;}
    CHECK(fixture_read_dungeon(legacy,old_length,22,&sentinel,&consumed)==0);
    CHECK(!catastrophe_active()&&catastrophe_acquired_count()==0);
    CHECK(fixture_read_dungeon(encoded,end-1,VERSION_EXTRA,&sentinel,&consumed)!=0);
    puts("Production dungeon-tail roundtrip, cells, events, craft, identities, legacy v22 and truncation: PASS");
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]
    for header in ('cave/cave-environment.h', 'cave/cave-flood.h', 'cave/cave-events.h',
                   'cave/cave-fixtures.h', 'cave/cave-water-flow.h', 'ui/question.h',
                   'player/player-song-internal.h'):
        prefix += f'\n#include "{header}"\n'
    # The SDL collector's private row layout is taken from production rather
    # than duplicating its ABI in this harness.
    screens = (ROOT / 'src/sdl/ui/sdl-screens.c').read_text(encoding='utf-8')
    row_start = screens.rfind('enum {', 0, screens.index('SDL_CHAR_SHEET_TEXT_LEN ='))
    row_end = screens.index('} sdl_char_sheet_line;') + len('} sdl_char_sheet_line;')
    prefix += '\n' + screens[row_start:row_end]
    prefix += '\nint sdl_char_sheet_collect_vitals(sdl_char_sheet_line*,int);\n'
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init = init[:init.index('    check_templates();')]
    source = OUT / 'check.c'
    source.write_text(prefix + fixture_function('terminal_extra') + '\n' +
                      fixture_function('reset_map') + '\n' + FRESH_MAP + '\n' + TESTS + '\n' + init +
                      '    test_probability();test_milestones();test_spread();test_force();test_escape();test_occupants_and_debug();test_front_events_and_reveal();test_real_events();test_reactions();test_cold_structures_and_supply();test_sheets();test_save();test_material_save_continuation();test_disconnected_front();\n'
                      '    printf("Catastrophe: %d checks PASS.\\n",checks);SDL_Quit();return 0;\n}\n', encoding='utf-8')
    writer, reader = OUT / 'writer.c', OUT / 'reader.c'
    writer.write_text(WRITER, encoding='utf-8')
    reader.write_text(READER, encoding='utf-8')
    objects = shlex.split((BUILD / 'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    objects = [p for p in objects if not p.endswith(('/src/main.c.obj', '/src/fs/save.c.obj', '/src/fs/load.c.obj'))]
    response = OUT / 'objects.rsp'
    response.write_text('\n'.join('"' + p + '"' for p in objects), encoding='utf-8')
    env = os.environ.copy()
    env['PATH'] = os.pathsep.join([*(str(BUILD / '_deps' / name) for name in
                                   ('SDL', 'SDL_ttf', 'SDL_image', 'SDL_mixer')),
                                  'C:/msys64/mingw64/bin', 'C:/msys64/usr/bin', env['PATH']])
    exe = OUT / 'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe', '-DUSE_SDL', '-std=c17', '-O0', '-g',
                    '@CMakeFiles/sil-more.dir/includes_C.rsp', str(source), str(writer), str(reader),
                    '@' + str(response), '@CMakeFiles/sil-more.dir/linkLibs.rsp',
                    '-Wl,--wrap=ui_question_ask', '-Wl,--wrap=object_intrinsic_difficulty', '-Wl,--wrap=cold_dam_pure', '-o', str(exe)],
                   cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix='data-', dir=OUT) as data:
        result = subprocess.run([str(exe), str(ROOT / 'lib/edit'), data], cwd=data,
                                env=env, capture_output=True, text=True, timeout=120)
        (OUT / 'validation.log').write_text(result.stdout + result.stderr, encoding='utf-8')
        print(result.stdout, end='')
        if result.returncode:
            print(result.stderr[-6000:])
            result.check_returncode()


def generated_levels():
    """Replay production generation plus catastrophe actions, with isolated data."""
    from check_utumno_generation import HARNESS
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / 'levels.c'
    source.write_text(HARNESS[:HARNESS.index('int main(int argc,char **argv)')] + r'''
static int dry_distance(void) {
    static int q[MAX_DUNGEON_HGT*MAX_DUNGEON_WID],dist[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    memset(dist,0,sizeof(dist));int head=0,tail=1;
    q[0]=p_ptr->py*MAX_DUNGEON_WID+p_ptr->px;dist[p_ptr->py][p_ptr->px]=1;
    while(head<tail) {
        int pos=q[head++],y=pos/MAX_DUNGEON_WID,x=pos%MAX_DUNGEON_WID;
        if(cave_feat[y][x]==FEAT_LESS||cave_feat[y][x]==FEAT_LESS_SHAFT)return dist[y][x]-1;
        for(int d=0;d<8;d++) {
            int ny=y+ddy_ddd[d],nx=x+ddx_ddd[d];
            if(!in_bounds_fully(ny,nx)||dist[ny][nx])continue;
            int f=cave_feat[ny][nx];
            if(!safe_tile(ny,nx)||f==FEAT_WATER||f==FEAT_DEEP_WATER
                ||catastrophe_owns(ny,nx))continue;
            dist[ny][nx]=dist[y][x]+1;q[tail++]=ny*MAX_DUNGEON_WID+nx;
        }
    }
    return -1;
}
int main(int argc,char **argv) {
    assert(argc==4);setbuf(stdout,NULL);log_set_level(LOG_ERROR);
    assert(SDL_Init(SDL_INIT_EVENTS));assert(term_init(&test_term,80,24,256)==0);
    test_term.xtra_hook=dummy_xtra;angband_term[0]=&test_term;Term_activate(&test_term);
    initialize(argv[1],argv[2]);small_maps=extra_stairs=false;
    static byte original[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    puts("depth,anger,kind,initial_dry_distance,first_lost_dry_route,changed_after_40,elapsed_ms");
    for(int depth=5;depth<=15;depth+=5)for(int anger=0;anger<=6;anger+=2) {
        wipe_o_list();wipe_mon_list();player_wipe();
        rp_ptr=&p_info[0];current_character_profile=&c_info[0];
        p_ptr->playing=true;p_ptr->chp=p_ptr->mhp=30000;
        p_ptr->depth=p_ptr->max_depth=depth;p_ptr->fixed_forge_count=3;
        p_ptr->on_the_run=true;p_ptr->morgoth_state=anger;
        playerturn=1;turn=1000;character_generated=true;Rand_state_init(900+depth);attempts=0;
        partition_passes=tunnel_passes=terrain_passes=population_passes=first_size=0;
        generate_cave();p_ptr->leaving=false;character_dungeon=true;
        /* Keep the observer on a stair; this measures the map and actor
         * simulation rather than pretending to be a human escape playtest. */
        cave_set_feat(p_ptr->py,p_ptr->px,FEAT_MORE);
        memcpy(original,cave_feat,sizeof(original));
        int initial=dry_distance(),lost=-1;
        Uint64 start=SDL_GetTicks();catastrophe_resume_level();
        assert(catastrophe_active());
        int kind=catastrophe_get_state().kind;
        for(int n=1;n<=40;n++) {
            if(n>1) {
                playerturn++;turn+=10;p_ptr->energy_use=100;
                catastrophe_begin_action();player_lava_begin_action();player_poison_terrain_begin_action();
                catastrophe_end_action();player_lava_end_action();player_poison_terrain_end_action();
            }
            cave_environment_process();
            if(initial>=0&&lost<0&&dry_distance()<0)lost=n;
            assert(!p_ptr->is_dead&&!p_ptr->leaving);
            assert(catastrophe_get_state().step==(unsigned)n);
        }
        int changed=0;
        for(int y=1;y<p_ptr->cur_map_hgt-1;y++)for(int x=1;x<p_ptr->cur_map_wid-1;x++) {
            changed+=original[y][x]!=cave_feat[y][x];
            if(original[y][x]==FEAT_WALL_PERM||original[y][x]==FEAT_LESS
                ||original[y][x]==FEAT_MORE)assert(original[y][x]==cave_feat[y][x]);
        }
        assert(changed>0);
        printf("%d,%d,%s,%d,%d,%d,%llu\n",depth,anger,catastrophe_name(kind),initial,lost,changed,
            (unsigned long long)(SDL_GetTicks()-start));
    }
    puts("12 generated-level catastrophe replays: PASS");SDL_Quit();return 0;
}
''', encoding='utf-8')
    objects = shlex.split((BUILD / 'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    objects = [p for p in objects if not p.endswith('/src/main.c.obj')]
    response = OUT / 'levels-objects.rsp'
    response.write_text('\n'.join('"' + p + '"' for p in objects), encoding='utf-8')
    env = os.environ.copy()
    env['PATH'] = os.pathsep.join([*(str(BUILD / '_deps' / name) for name in
                                   ('SDL', 'SDL_ttf', 'SDL_image', 'SDL_mixer')),
                                  'C:/msys64/mingw64/bin', 'C:/msys64/usr/bin', env['PATH']])
    exe = OUT / 'levels.exe'
    wraps = ['get_sdl_config_path', 'level_gen_screen_start_attempt', 'process_player',
             'apply_quadrant_generation_modes', 'connect_rooms_stairs', 'place_dungeon_terrain',
             'run_partition_monster_pass']
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe', '-DUSE_SDL', '-std=c17', '-O1',
                    '@CMakeFiles/sil-more.dir/includes_C.rsp', str(source), '@' + str(response),
                    '@CMakeFiles/sil-more.dir/linkLibs.rsp',
                    *('-Wl,--wrap=' + name for name in wraps), '-o', str(exe)],
                   cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix='levels-data-', dir=OUT) as data:
        result = subprocess.run([str(exe), str(ROOT), data, str(OUT)], cwd=data, env=env,
                                capture_output=True, text=True, timeout=300)
        (OUT / 'levels.log').write_text(result.stdout + result.stderr, encoding='utf-8')
        print(result.stdout, end='')
        if result.returncode:
            print(result.stderr[-6000:])
            result.check_returncode()


if __name__ == '__main__':
    generated_levels() if '--levels' in sys.argv else main()

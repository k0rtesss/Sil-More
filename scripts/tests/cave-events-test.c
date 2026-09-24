#include "angband.h"
#include "externs.h"
#include "cave/cave-events.h"
#include "cave/cave-water-flow.h"
#include "cave/cave-fixtures.h"
#include <stdio.h>
#include <stdarg.h>
static byte features[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static int checks, notices, interruptions;
static char last_message[256];
#define CHECK(t) do { checks++; if (!(t)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#t); exit(1); } } while (0)
void msg_format(cptr fmt, ...) { va_list ap; va_start(ap,fmt); vsnprintf(last_message,sizeof(last_message),fmt,ap); va_end(ap); notices++; }
void msg_print(cptr text) { if(text)msg_format("%s",text); }
void disturb(int a,int b) { (void)a; (void)b; interruptions++; }
void sound_at(int val, int y, int x) { (void)val; (void)y; (void)x; }
static void check_diagonal_fixture_audio(void)
{
    /* The reported fixture has floor diagonally beside its corner, with
     * blocked cardinal neighbours. Sound emitted from that exposed corner
     * must reach the room. Rotate the arrangement through every facing. */
    for (int facing=0; facing<4; ++facing)
    {
        for(int y=0;y<15;y++) for(int x=0;x<22;x++) features[y][x]=FEAT_WALL_EXTRA;
        cave_events_reset(); cave_fixtures_clear(); cave_water_flow_reset();
        int cy=7, cx=10;
        int rubble_y=cy, rubble_x=cx-1;
        int listener_y=cy-1, listener_x=cx-3;
        int near_y=cy-1, near_x=cx-1;
        for(int dy=-1;dy<=0;dy++) for(int dx=-3;dx<=-1;dx++)
        {
            int ry=dy, rx=dx;
            for(int i=0;i<facing;i++) { int t=ry; ry=rx; rx=-t; }
            features[cy+ry][cx+rx]=FEAT_FLOOR;
        }
        for(int i=0;i<facing;i++)
        {
            int t=rubble_y-cy; rubble_y=cy+rubble_x-cx; rubble_x=cx-t;
            t=listener_y-cy; listener_y=cy+listener_x-cx; listener_x=cx-t;
            t=near_y-cy; near_y=cy+near_x-cx; near_x=cx-t;
        }
        features[rubble_y][rubble_x]=FEAT_RUBBLE;
        cave_fixture_set(cy,cx,CAVE_FIXTURE_BRAZIER);
        cave_events_terrain_changed();
        CHECK(cave_audio_distance(cy,cx,listener_y,listener_x)==3);
        CHECK(cave_fixture_sound_gain_at(listener_y,listener_x)==0.25f);
        CHECK(cave_audio_distance(cy,cx,near_y,near_x)==1);
        CHECK(cave_fixture_sound_gain_at(near_y,near_x)==1.0f);
        cave_fixture_set(cy,cx,CAVE_FIXTURE_WALL_TORCH);
        CHECK(cave_fixture_sound_gain_at(near_y,near_x)==1.0f);
        cave_fixture_set(cy,cx,CAVE_FIXTURE_WALL_TORCH_2);
        CHECK(cave_fixture_sound_gain_at(near_y,near_x)==1.0f);
        cave_fixture_set(cy,cx,CAVE_FIXTURE_BRAZIER);
        /* This is diagonal exposure, not special transmission through rubble. */
        features[rubble_y][rubble_x]=FEAT_WALL_EXTRA; cave_events_terrain_changed();
        CHECK(cave_audio_distance(cy,cx,listener_y,listener_x)==3);
        CHECK(cave_fixture_sound_gain_at(listener_y,listener_x)==0.25f);
        features[rubble_y][rubble_x]=FEAT_FLOOR; cave_events_terrain_changed();
        CHECK(cave_audio_distance(cy,cx,listener_y,listener_x)==3);
        CHECK(cave_fixture_sound_gain_at(listener_y,listener_x)==0.25f);
        /* With no exposed open neighbour left, the fixture is truly sealed. */
        for(int dy=-1;dy<=1;dy++) for(int dx=-1;dx<=1;dx++)
            features[cy+dy][cx+dx]=FEAT_WALL_EXTRA;
        cave_events_terrain_changed();
        CHECK(cave_audio_distance(cy,cx,listener_y,listener_x)==-1);
        CHECK(cave_fixture_sound_gain_at(listener_y,listener_x)==0.0f);
    }
}
int main(void)
{
    cave_world_event e, saved[CAVE_EVENTS_MAX];
    cave_feat = features; p_ptr->cur_map_hgt=15; p_ptr->cur_map_wid=22;
    p_ptr->py=5; p_ptr->px=3; turn=100;
    for(int y=0;y<15;y++) for(int x=0;x<22;x++)
        features[y][x]=(!y||!x||y==14||x==21||x==11)?FEAT_WALL_PERM:FEAT_FLOOR;
    cave_events_reset(); cave_water_flow_reset();
    CHECK(cave_audio_distance(5,8,5,3)==5);
    CHECK(cave_audio_distance(5,18,5,3)==-1);
    CHECK(cave_audio_distance(5,8,5,4)==4); /* listener cache moves */
    cave_event_emit(CAVE_EVENT_FIGHT,5,8,24);
    CHECK(cave_fighting_mask_at(5,8)==8);
    CHECK(cave_fighting_mask_at(5,3)>0);
    CHECK(cave_fighting_mask_at(5,18)==0);
    cave_events_process();
    CHECK(strstr(last_message,"shouts and clashing weapons to the east")!=NULL);
    memcpy(saved,cave_events_state(),sizeof(saved));
    cave_events_restore(saved,cave_events_next_serial());
    CHECK(cave_fighting_mask_at(5,8)==8);
    turn+=60; CHECK(cave_fighting_mask_at(5,8)==0);
    turn=100; cave_events_reset(); notices=interruptions=0;
    cave_event_emit(CAVE_EVENT_BUILD,5,8,20);
    CHECK(cave_event_for_listener(5,3,10,0,&e)); CHECK(e.y==5 && e.x==8);
    CHECK(!cave_event_for_listener(5,18,100,0,&e));
    CHECK(cave_sound_mask_at(5,3)>0); CHECK(cave_sound_mask_at(5,18)==0);
    cave_events_process(); CHECK(notices==1 && interruptions==0);
    CHECK(strstr(last_message,"hammering to the east")!=NULL);
    cave_events_process(); CHECK(notices==1);
    cave_event_emit(CAVE_EVENT_BUILD,5,8,20); cave_events_process(); CHECK(notices==1);
    cave_event_emit(CAVE_EVENT_WARNING,5,4,20); cave_events_process(); CHECK(interruptions==1);
    memcpy(saved,cave_events_state(),sizeof(saved));
    cave_events_restore(saved,cave_events_next_serial()); cave_events_process(); CHECK(notices==2);
    turn+=60; CHECK(!cave_event_for_listener(5,3,10,0,&e)); CHECK(cave_sound_mask_at(5,3)==0);
    cave_events_reset(); features[5][4]=FEAT_WATER;
    CHECK(cave_sound_mask_at(5,3)==0);
    cave_water_flow_set(5,4,CAVE_WATER_FLOW_EAST); cave_events_terrain_changed();
    CHECK(cave_sound_mask_at(5,3)==7); CHECK(cave_sound_mask_at(5,18)==0);
    cave_water_flow_reset(); cave_events_terrain_changed();
    /* Single hallway: a closed door absorbs four extra sound units. */
    for(int y=0;y<15;y++) for(int x=0;x<22;x++) features[y][x]=FEAT_WALL_PERM;
    for(int x=2;x<=8;x++) features[5][x]=FEAT_FLOOR;
    cave_event_emit(CAVE_EVENT_DIG,5,2,12); cave_events_terrain_changed();
    CHECK(cave_sound_mask_at(5,8)==2);
    CHECK(cave_audio_distance(5,8,5,2)==6);
    features[5][5]=FEAT_DOOR_HEAD; cave_events_terrain_changed();
    CHECK(cave_sound_mask_at(5,8)==0);
    CHECK(cave_audio_distance(5,8,5,2)==10);
    features[5][5]=FEAT_WALL_PERM; cave_events_terrain_changed();
    CHECK(!cave_event_for_listener(5,8,100,0,&e));
    CHECK(cave_audio_distance(5,8,5,2)==-1);
    cave_events_reset(); features[5][5]=FEAT_FLOOR; cave_events_terrain_changed();
    cave_event_emit(CAVE_EVENT_FIGHT,5,2,24);
    CHECK(cave_fighting_mask_at(5,8)==6);
    features[5][5]=FEAT_DOOR_HEAD; cave_events_terrain_changed();
    CHECK(cave_fighting_mask_at(5,8)==4);
    features[5][5]=FEAT_WALL_PERM; cave_events_terrain_changed();
    CHECK(cave_fighting_mask_at(5,8)==0);
    cave_events_reset(); features[7][7]=FEAT_FLOOR; features[8][8]=FEAT_FLOOR;
    cave_event_emit(CAVE_EVENT_COLLAPSE,7,7,30); cave_events_terrain_changed();
    CHECK(!cave_event_for_listener(8,8,100,0,&e));
    CHECK(cave_audio_distance(8,8,7,7)==-1);
    /* An exposed single corner carries playback diagonally. The separate
     * gameplay field still needs two steps and keeps its old hearing rules. */
    features[8][7]=FEAT_FLOOR; cave_events_terrain_changed();
    CHECK(cave_audio_distance(8,8,7,7)==1);
    cave_events_reset(); cave_event_emit(CAVE_EVENT_BUILD,7,7,2);
    CHECK(!cave_event_for_listener(8,8,100,0,&e));
    features[7][7]=FEAT_WATER; cave_water_flow_set(7,7,CAVE_WATER_FLOW_EAST);
    cave_events_terrain_changed();
    CHECK(cave_flowing_water_sound_level_at(8,8)==6);
    features[8][7]=FEAT_DOOR_HEAD; cave_events_terrain_changed();
    CHECK(cave_audio_distance(8,8,7,7)==6); /* closed door still absorbs sound */
    CHECK(cave_flowing_water_sound_level_at(8,8)==1);
    features[7][7]=FEAT_FLOOR; features[8][7]=FEAT_WALL_PERM;
    cave_water_flow_reset(); cave_events_terrain_changed();
    cave_events_player_moved(true); CHECK(cave_events_player_is_moving());
    cave_events_reset(); CHECK(!cave_events_player_is_moving());
    /* Default-off debug logging covers every emission, even behind walls,
     * during same-kind cooldowns and when a burst exceeds the sound ring. */
    CHECK(!option_norm[OPT_show_dungeon_events]);
    CHECK(strcmp(option_text[OPT_show_dungeon_events], "show_dungeon_events")==0);
    bool listed=false;
    for(int i=0;i<OPT_PAGE_PER;i++) {
        if(option_page[INTERFACE_PAGE][i]==OPT_show_dungeon_events) listed=true;
        CHECK(option_page[DEBUG_PAGE][i]!=OPT_show_dungeon_events);
    }
    CHECK(listed);
    notices=0;
    cave_event_emit(CAVE_EVENT_BUILD,7,7,8); CHECK(notices==0);
    op_ptr->opt[OPT_show_dungeon_events]=true;
    cave_events_reset();
    for(int kind=CAVE_EVENT_NONE+1;kind<CAVE_EVENT_MAX;kind++) {
        cave_event_emit(kind,7,7,8);
        CHECK(notices==kind);
        CHECK(strstr(last_message,"[Dungeon event ")!=NULL);
        CHECK(strstr(last_message,"(y=7, x=7), turn 160, volume 8")!=NULL);
    }
    for(int i=0;i<CAVE_EVENTS_MAX+3;i++) cave_event_emit(CAVE_EVENT_BUILD,7,7,8);
    CHECK(notices==CAVE_EVENT_MAX-1+CAVE_EVENTS_MAX+3);
    int logged=notices;
    cave_events_process(); cave_events_process(); CHECK(notices==logged);
    CHECK(!cave_event_for_listener(5,3,100,0,&e));
    cave_event_emit(CAVE_EVENT_NONE,7,7,8);
    cave_event_emit(CAVE_EVENT_BUILD,-1,7,8); CHECK(notices==logged);
    op_ptr->opt[OPT_show_dungeon_events]=false;
    cave_event_emit(CAVE_EVENT_BUILD,7,7,8); CHECK(notices==logged);
    cave_event_emit(CAVE_EVENT_FREEZE,8,8,8);
    CHECK(cave_event_latest(&e) && e.kind==CAVE_EVENT_FREEZE && e.y==8 && e.x==8);
    CHECK(e.serial==cave_events_next_serial()-1);
    turn+=600;
    CHECK(cave_event_latest(&e));
    memcpy(saved,cave_events_state(),sizeof(saved));
    cave_events_restore(saved,cave_events_next_serial());
    CHECK(cave_event_latest(&e) && e.kind==CAVE_EVENT_FREEZE);
    CHECK(!cave_event_for_listener(8,8,100,0,&e));
    CHECK(cave_sound_mask_at(8,8)==0);
    cave_events_process(); CHECK(notices==logged);
    cave_events_reset(); CHECK(!cave_event_latest(&e));
    /* Playback ranges and fixture normalization use the real terrain paths. */
    for(int y=0;y<15;y++) for(int x=0;x<22;x++)
        features[y][x]=(!y||!x||y==14||x==21)?FEAT_WALL_PERM:FEAT_FLOOR;
    features[5][5]=FEAT_WATER;
    cave_water_flow_set(5,5,CAVE_WATER_FLOW_EAST);
    cave_events_terrain_changed();
    CHECK(cave_flowing_water_sound_level_at(5,5)==7);
    CHECK(cave_flowing_water_sound_level_at(5,11)==1);
    CHECK(cave_flowing_water_sound_level_at(5,12)==0);
    CHECK(sound_level_gain(1,7)>0.027f && sound_level_gain(1,7)<0.029f);
    CHECK(sound_level_gain(0,7)==0.0f);
    CHECK(sound_level_gain(6,7)==1.0f);
    features[5][5]=FEAT_WALL_EXTRA;
    cave_fixture_set(5,5,CAVE_FIXTURE_WALL_TORCH);
    cave_events_terrain_changed();
    CHECK(cave_fixture_sound_gain_at(5,6)==1.0f);
    CHECK(cave_fixture_sound_gain_at(5,8)>0.110f && cave_fixture_sound_gain_at(5,8)<0.112f);
    CHECK(cave_fixture_sound_gain_at(5,9)==0.0f);
    cave_fixture_set(5,5,CAVE_FIXTURE_BRAZIER);
    CHECK(cave_fixture_sound_gain_at(5,6)==1.0f);
    CHECK(cave_fixture_sound_gain_at(5,9)==0.0625f);
    CHECK(cave_fixture_sound_gain_at(5,10)==0.0f);
    features[5][8]=FEAT_DOOR_HEAD; cave_events_terrain_changed();
    CHECK(cave_audio_distance(5,8,5,9)==1); /* door emits at its own face */
    CHECK(cave_audio_distance(-1,0,5,9)==-1);
    check_diagonal_fixture_audio();
    printf("World sound tests: %d checks passed.\n",checks); return 0;
}

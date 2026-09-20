/* Standalone decision harness; see compile command in task validation. */
#include "angband.h"
#include "monster/monster-world.h"
#include "cave/cave-environment.h"
#include "cave/cave-events.h"
#include <assert.h>

static monster_type monsters[4];
static monster_race races[2];
static monster_lore lore[2];
static player_type player;
static byte features[32][MAX_DUNGEON_WID];
static s16b occupants[32][MAX_DUNGEON_WID];
monster_type* mon_list = monsters;
monster_race* r_info = races;
monster_lore* l_list = lore;
player_type* p_ptr = &player;
byte (*cave_feat)[MAX_DUNGEON_WID] = features;
s16b (*cave_m_idx)[MAX_DUNGEON_WID] = occupants;
s16b mon_max = 4;
const s16b ddx[10] = {0,-1,0,1,-1,0,1,-1,0,1};
const s16b ddy[10] = {0,1,1,1,0,0,0,-1,-1,-1};
static bool combat, heard, repair, rejected, blocked;
static int progress, emitted, moves, hazard;
int distance(int a,int b,int c,int d) { return MAX(ABS(a-c),ABS(b-d)); }
bool los(int a,int b,int c,int d) { (void)a;(void)b;(void)c;(void)d;return true; }
int monster_skill(monster_type* m,int s) { (void)m;(void)s;return 10; }
bool monster_ai_can_see_player(const monster_type* m) { (void)m;return combat; }
bool monster_senses_target(const monster_type* m,int* y,int* x)
{ (void)m;(void)y;(void)x;return combat; }
int cave_environment_pending_hazard(int y,int x) { return y==10&&x==10 ? hazard:0; }
bool cave_environment_bridge_job_at(int y,int x,environment_bridge_job* job)
{
    if (y!=10||x!=11||progress>=4) return false;
    memset(job,0,sizeof(*job));job->material=ENV_BRIDGE_WOOD;job->repair=repair;
    return true;
}
bool cave_environment_job_safe(int y,int x) { (void)y;(void)x;return true; }
int cave_environment_bridge_progress(int y,int x) { (void)y;(void)x;return progress; }
bool cave_environment_bridge_work(int y,int x,int material)
{ (void)y;(void)x;(void)material;if(!rejected)++progress;return progress>=4; }
bool cave_event_for_listener(int y,int x,int p,u32b after,cave_world_event* event)
{
    (void)y;(void)x;(void)p;
    if(!heard||after>=7)return false;
    memset(event,0,sizeof(*event));event->serial=7;event->kind=CAVE_EVENT_COLLAPSE;
    event->y=10;event->x=13;return true;
}
void cave_event_emit(int k,int y,int x,int v)
{ assert(k==CAVE_EVENT_BUILD);assert(y==10&&x==11);(void)v;++emitted; }
bool cave_exist_mon(monster_race* r,int y,int x,bool o,bool d)
{ (void)r;(void)o;(void)d;return !blocked&&!occupants[y][x]&&features[y][x]!=FEAT_CHASM; }
void process_move(monster_type* m,int y,int x,bool bash)
{ (void)bash;++moves;m->fy=y;m->fx=x; }
static monster_type* reset(void)
{
    memset(monsters,0,sizeof(monsters));memset(races,0,sizeof(races));
    memset(&player,0,sizeof(player));memset(features,0,sizeof(features));
    player.cur_map_hgt=player.cur_map_wid=32;player.py=25;player.px=25;
    monsters[1].r_idx=1;monsters[1].fy=monsters[1].fx=10;
    monsters[1].alertness=ALERTNESS_UNWARY;
    races[1].flags5=RF5_BRIDGE_BUILDER;
    combat=heard=rejected=blocked=false;repair=true;progress=emitted=moves=hazard=0;
    monster_world_observe(&monsters[1]);return &monsters[1];
}
int main(void)
{
    monster_type* m=reset();int i;
    m->target_y=3;m->target_x=4;
    for(i=0;i<4;++i)assert(monster_world_turn(m));
    assert(progress==4&&emitted==4&&m->world.supplies==20);
    assert(m->target_y==3&&m->target_x==4&&m->ai.sense.kind==0);
    m=reset();combat=true;assert(!monster_world_turn(m));assert(!progress);
    m=reset();races[1].flags2=RF2_MINDLESS;assert(!monster_world_turn(m));
    m=reset();races[1].flags1=RF1_PEACEFUL;assert(!monster_world_turn(m));
    m=reset();rejected=true;for(i=0;i<3;++i)assert(monster_world_turn(m));
    assert(m->world.task==MON_WORLD_NONE&&m->world.supplies==24);
    m=reset();repair=false;assert(!monster_world_turn(m));
    heard=true;m->alertness=ALERTNESS_UNWARY-5;monster_world_observe(m);
    assert(m->alertness==ALERTNESS_UNWARY&&m->world.observation_x==13);
    assert(monster_world_turn(m)&&progress==1&&m->ai.sense.kind==0);
    m=reset();hazard=FEAT_CHASM;races[1].flags2=RF2_MINDLESS;
    assert(monster_world_turn(m)&&moves==1);
    m=reset();repair=false;heard=true;blocked=true;monster_world_observe(m);
    races[1].flags5=0;for(i=0;i<3;++i)assert(monster_world_turn(m));
    assert(m->world.task==MON_WORLD_NONE);
    puts("monster world decisions: PASS");return 0;
}

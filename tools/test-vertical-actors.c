/* Production current/fall implementation with injected movement/cache/damage
 * boundaries. Link monster-move.c too: its REAL m_fall_in_chasm is executed at
 * the displacement boundary, including the legacy deletion fallback. Only the
 * movement prefix is injected; the formerly missed fall chain is not mocked. */
#define monster_swap fixture_monster_swap
#include "../src/cave/cave-vertical-actors.c"
#undef monster_swap
#include <assert.h>

static player_type player;
static maxima limits;
static feature_type feature_definitions[256];
static monster_type active_monsters[MAX_MONSTERS];
static monster_race races[3];
static object_type active_objects[8], remote_objects[8];
static level_image remote;
static cave_vertical_state vertical;
static byte features[32][MAX_DUNGEON_WID];
static u16b information[32][256];
static s16b occupants[32][MAX_DUNGEON_WID];
_Thread_local player_type* p_ptr=&player;
_Thread_local monster_type* mon_list=active_monsters;
_Thread_local monster_race* r_info=races;
_Thread_local object_type* o_list=active_objects;
_Thread_local byte (*cave_feat)[MAX_DUNGEON_WID]=features;
_Thread_local u16b (*cave_info)[256]=information;
_Thread_local s16b (*cave_m_idx)[MAX_DUNGEON_WID]=occupants;
_Thread_local s16b mon_max,mon_cnt,o_max,o_cnt,num_repro;
maxima* z_info=&limits;
feature_type* f_info=feature_definitions;
char combat_roll_special_char;
byte combat_roll_special_attr;
int silent_passage_exchange_target;
const s16b ddx[10]={0,-1,0,1,-1,0,1,-1,0,1};
const s16b ddy[10]={0,1,1,1,0,0,0,-1,-1,-1};
static bool ready;
static int requests, damage_calls, deaths, removals, updates, legacy_deletions, damage_who;
static int impact_calls, impact_levels, impact_feature, last_damage;
static int flow_strength;

const cave_vertical_state* cave_vertical_get(void) { return &vertical; }
int cave_vertical_current(int y,int x,int* direction)
{
    (void)y;*direction=x<12?6:5;
    return flow_strength;
}
struct level_image* level_cache_mutable_floor(int depth)
{ return ready&&depth==2?&remote:NULL; }
bool level_cache_request(int depth) { assert(depth==2);requests++;return true; }
int damroll(int dice,int sides) { assert((dice==3||dice==6)&&sides==4);return dice*sides; }
/* Inject the shared engine boundary; root's production-water fixture verifies
 * its dice/rounding rules. These distinct outcomes verify receiver selection. */
int cave_vertical_fall_damage(int levels,int landing_feature)
{
    impact_calls++;impact_levels=levels;impact_feature=landing_feature;
    assert(levels==1); /* Resident gravity connections are adjacent floors. */
    return landing_feature==FEAT_DEEP_WATER?6:landing_feature==FEAT_WATER?9:12;
}
bool mon_take_hit(int index,int damage,cptr note,int who)
{
    monster_type* monster=&mon_list[index];
    assert(note&&(who==0||who==-1));damage_who=who;last_damage=damage;
    damage_calls++;monster->hp-=damage;
    if(monster->hp>0)return false;
    deaths++;occupants[monster->fy][monster->fx]=0;
    races[monster->r_idx].cur_num--;
    if(races[monster->r_idx].flags1&RF1_UNIQUE)races[monster->r_idx].max_num=0;
    memset(monster,0,sizeof(*monster));mon_cnt--;return true;
}
void target_set_monster(int index) { player.target_who=index; }
void health_track(int index) { player.health_who=index; }
void song_disguise_handle_monster_removed(int index) { assert(index==1);removals++; }
void song_duels_handle_monster_removed(int index) { assert(index==1);removals++; }
void monster_social_remap(int old_index,int new_index)
{ assert(old_index==1&&new_index==0);removals++; }
void update_mon(int index,bool full) { assert(index==1&&full);updates++; }
void fixture_monster_swap(int y,int x,int yy,int xx)
{
    int index=occupants[y][x];assert(index==1&&occupants[yy][xx]==0);
    occupants[y][x]=0;occupants[yy][xx]=index;
    mon_list[index].fy=yy;mon_list[index].fx=xx;
    monster_abilities_forced_movement(&mon_list[index]);updates++;
    m_fall_in_chasm(yy,xx); /* Real production function from monster-move.c. */
}
void lite_spot(int y,int x) { (void)y;(void)x; }
void msg_print(cptr message) { (void)message; }
void msg_format(cptr message,...) { (void)message; }
void message_flush(void) {}
void monster_desc(char* desc,size_t size,const monster_type* m,int mode)
{ (void)m;(void)mode;snprintf(desc,size,"fixture monster"); }
void update_combat_rolls1b(const monster_type* a,const monster_type* b,bool vis)
{ (void)a;(void)b;(void)vis; }
void update_combat_rolls2(int dd,int ds,int dam,int pd,int ps,int prot,int percent,int type,bool melee)
{ (void)dd;(void)ds;(void)dam;(void)pd;(void)ps;(void)prot;(void)percent;(void)type;(void)melee; }
void quest_followup_kill(int race) { (void)race; }
void monster_death(int index) { (void)index;deaths++; }
void delete_monster_idx(int index)
{
    monster_type* monster=&mon_list[index];legacy_deletions++;
    occupants[monster->fy][monster->fx]=0;races[monster->r_idx].cur_num--;
    memset(monster,0,sizeof(*monster));mon_cnt--;
}
void delete_monster(int y,int x) { delete_monster_idx(occupants[y][x]); }

static monster_type* reset_fixture(int x)
{
    memset(&player,0,sizeof(player));memset(active_monsters,0,sizeof(active_monsters));
    memset(races,0,sizeof(races));memset(active_objects,0,sizeof(active_objects));
    memset(remote_objects,0,sizeof(remote_objects));memset(&remote,0,sizeof(remote));
    memset(&vertical,0,sizeof(vertical));memset(features,0,sizeof(features));
    memset(information,0,sizeof(information));memset(occupants,0,sizeof(occupants));
    player.py=player.px=25;player.cur_map_hgt=player.cur_map_wid=32;player.depth=1;
    limits.o_max=8;mon_max=2;mon_cnt=1;o_max=1;o_cnt=num_repro=0;
    active_monsters[1].r_idx=1;active_monsters[1].hp=30;
    active_monsters[1].fy=10;active_monsters[1].fx=x;
    active_monsters[1].energy=50;occupants[10][x]=1;races[1].cur_num=1;
    for(int xx=9;xx<15;xx++)features[10][xx]=FEAT_WATER;
    remote.depth=2;remote.hgt=remote.wid=32;remote.mon_max=remote.o_max=1;
    remote.objects=remote_objects;remote.feat[10][10]=FEAT_FLOOR;
    vertical.portal_count=1;vertical.portals[0]=(cave_vertical_portal){
        .identity=77,.y=10,.x=12,.peer_depth=2,.peer_y=10,.peer_x=10,
        .kind=CAVE_VERTICAL_FALL,.material=FEAT_WATER,.strength=3,
        .flags=CAVE_VERTICAL_DOWN};
    ready=true;flow_strength=3;requests=damage_calls=deaths=removals=updates=legacy_deletions=damage_who=0;
    impact_calls=impact_levels=impact_feature=last_damage=0;
    cave_vertical_monster_begin_action(1);
    return &active_monsters[1];
}

static void give_carried_items(void)
{
    active_monsters[1].hold_o_idx=1;o_max=3;o_cnt=2;
    active_objects[1].k_idx=3;active_objects[1].number=4;
    active_objects[1].held_m_idx=1;active_objects[1].next_o_idx=2;
    active_objects[2].k_idx=4;active_objects[2].number=7;
    active_objects[2].held_m_idx=1;
}

int main(void)
{
    monster_type* monster=reset_fixture(10);races[1].flags2=RF2_FLYING;
    cave_vertical_monster_end_action(1);
    assert(monster->fx==10&&!damage_calls&&mon_cnt==1); /* airborne immunity */

    monster=reset_fixture(10);occupants[10][11]=2;
    cave_vertical_monster_end_action(1);assert(monster->fx==10&&!damage_calls);
    monster=reset_fixture(10);features[10][10]=FEAT_FLOOR;
    cave_vertical_monster_end_action(1);assert(monster->fx==10&&!damage_calls);

    monster=reset_fixture(10);cave_vertical_monster_end_action(1);
    assert(monster->fx==11&&occupants[10][11]==1&&!occupants[10][10]);
    assert(monster->energy==50&&!damage_calls);
    cave_vertical_monster_end_action(1);assert(monster->fx==11&&!damage_calls);
    cave_vertical_monster_begin_action(1);cave_vertical_monster_end_action(1);
    assert(!monster->r_idx&&damage_calls==1&&remote.mon_cnt==1);
    assert(remote.monsters[1].hp==18&&remote.monsters[1].energy==50);
    cave_vertical_monster_end_action(1);assert(damage_calls==1); /* one floor/action */

    monster=reset_fixture(11);ready=false;
    cave_vertical_monster_end_action(1);assert(monster->fx==11&&monster->hp==30);
    assert(requests==1&&!damage_calls&&mon_cnt==1);
    cave_vertical_monster_end_action(1);assert(requests==1);
    ready=true;cave_vertical_monster_begin_action(1);cave_vertical_monster_end_action(1);
    assert(!monster->r_idx&&damage_calls==1&&remote.mon_cnt==1);

    monster=reset_fixture(11);remote.monsters_at[10][10]=1;
    cave_vertical_monster_end_action(1);assert(monster->fx==11&&!damage_calls);
    monster=reset_fixture(11);remote.mon_max=MAX_MONSTERS;
    for(int i=1;i<MAX_MONSTERS;i++)remote.monsters[i].r_idx=2;
    cave_vertical_monster_end_action(1);assert(monster->fx==11&&monster->hp==30);
    assert(!damage_calls&&!impact_calls&&mon_cnt==1);
    monster=reset_fixture(11);give_carried_items();remote.o_max=8;
    for(int i=1;i<8;i++)remote_objects[i].k_idx=2;
    cave_vertical_monster_end_action(1);
    assert(monster->fx==11&&monster->hp==30&&o_cnt==2&&!damage_calls&&!impact_calls);

    monster=reset_fixture(12);give_carried_items();races[1].flags1=RF1_UNIQUE;
    player.target_who=player.health_who=1;
    silent_passage_exchange_target=1;
    monster->world.initialized=1;monster->world.supplies=9;monster->world.task=2;
    cave_vertical_monster_end_action(1);
    assert(!monster->r_idx&&mon_cnt==0&&remote.mon_cnt==1&&races[1].cur_num==1);
    assert(remote.monsters[1].hp==18&&remote.monsters[1].energy==50);
    assert(!occupants[10][12]&&remote.monsters_at[10][10]==1);
    assert(o_cnt==0&&remote.o_cnt==2&&!active_objects[1].k_idx&&!active_objects[2].k_idx);
    assert(remote_objects[1].number+remote_objects[2].number==11);
    assert(remote_objects[1].held_m_idx==1&&remote_objects[2].held_m_idx==1);
    assert(remote.monsters[1].hold_o_idx==2&&remote_objects[2].next_o_idx==1);
    assert(!player.target_who&&!player.health_who&&removals==3);
    assert(!silent_passage_exchange_target&&!remote.monsters[1].world.task);
    assert(remote.monsters[1].world.supplies==9&&remote.monsters[1].world.home_x==10);

    monster=reset_fixture(12);races[1].flags1=RF1_UNIQUE;monster->hp=5;
    cave_vertical_monster_end_action(1);cave_vertical_monster_end_action(1);
    assert(damage_calls==1&&deaths==1&&races[1].max_num==0&&races[1].cur_num==0);
    assert(!remote.mon_cnt&&mon_cnt==0); /* canonical death, no second deletion */

    monster=reset_fixture(12);vertical.portals[0].kind=CAVE_VERTICAL_RAPIDS;
    cave_vertical_monster_end_action(1);
    assert(!damage_calls&&!monster->r_idx&&remote.monsters[1].hp==30);
    assert(races[1].cur_num==0); /* ordinary population follows active map */
    int excluded[]={CAVE_VERTICAL_STREAM,CAVE_VERTICAL_SUMP,CAVE_VERTICAL_CONTROL,CAVE_VERTICAL_TUBE};
    for(unsigned i=0;i<sizeof(excluded)/sizeof(excluded[0]);i++)
    {
        monster=reset_fixture(12);vertical.portals[0].kind=excluded[i];
        cave_vertical_monster_end_action(1);assert(monster->r_idx==1&&!damage_calls);
    }
    monster=reset_fixture(12);vertical.portals[0].flags|=CAVE_VERTICAL_CLOSED;
    cave_vertical_monster_end_action(1);assert(monster->r_idx==1&&!damage_calls);

    /* Actual m_fall_in_chasm integration: the old path deleted these healthy
     * survivors before the action-end hook could see them. */
    int hole_kinds[]={CAVE_VERTICAL_LAKE,CAVE_VERTICAL_FALL,CAVE_VERTICAL_LAVA_FALL};
    for(unsigned i=0;i<sizeof(hole_kinds)/sizeof(hole_kinds[0]);i++)
    {
        monster=reset_fixture(11);give_carried_items();races[1].flags1=RF1_UNIQUE;
        vertical.portals[0].kind=hole_kinds[i];features[10][12]=FEAT_CHASM;
        fixture_monster_swap(10,11,10,12);
        assert(!monster->r_idx&&remote.mon_cnt==1&&remote.monsters[1].hp==18);
        assert(remote.o_cnt==2&&o_cnt==0&&races[1].cur_num==1&&!legacy_deletions);
        cave_vertical_monster_end_action(1);assert(damage_calls==1&&!legacy_deletions);
    }
    monster=reset_fixture(11);features[10][12]=FEAT_CHASM;ready=false;
    fixture_monster_swap(10,11,10,12);
    assert(monster->fx==12&&monster->hp==30&&!damage_calls&&!legacy_deletions&&requests==1);
    cave_vertical_monster_end_action(1);assert(requests==1); /* no second attempt */
    cave_vertical_monster_begin_action(1);assert(monster->skip_next_turn);
    cave_vertical_monster_end_action(1);assert(requests==2&&!legacy_deletions);
    ready=true;cave_vertical_monster_begin_action(1);assert(monster->skip_next_turn);
    monster->skip_next_turn=false; /* Authoritative scheduler consumes its skip. */
    cave_vertical_monster_end_action(1);
    assert(!monster->r_idx&&damage_calls==1&&remote.mon_cnt==1&&!legacy_deletions);

    monster=reset_fixture(11);features[10][12]=FEAT_CHASM;remote.monsters_at[10][10]=2;
    fixture_monster_swap(10,11,10,12);
    assert(monster->fx==12&&monster->hp==30&&!legacy_deletions&&!damage_calls);
    monster=reset_fixture(11);features[10][12]=FEAT_CHASM;vertical.portal_count=0;
    fixture_monster_swap(10,11,10,12);
    assert(!monster->r_idx&&legacy_deletions==1&&!remote.mon_cnt); /* original generic chasm */
    monster=reset_fixture(11);features[10][12]=FEAT_CHASM;races[1].flags2=RF2_FLYING;
    fixture_monster_swap(10,11,10,12);
    assert(monster->r_idx==1&&!legacy_deletions&&!damage_calls);
    cave_vertical_monster_begin_action(1);assert(!monster->skip_next_turn);

    monster=reset_fixture(11);features[10][12]=FEAT_CHASM;
    monster->mflag|=MFLAG_PLAYER_PUSH;fixture_monster_swap(10,11,10,12);
    assert(damage_who==-1&&remote.mon_cnt==1);
    assert(!(remote.monsters[1].mflag&MFLAG_PLAYER_PUSH));

    monster=reset_fixture(11);features[10][11]=features[10][12]=FEAT_POISON;
    vertical.portals[0].material=FEAT_POISON;
    cave_vertical_monster_end_action(1);
    assert(!monster->r_idx&&remote.mon_cnt==1&&damage_calls==1&&!legacy_deletions);

    monster=reset_fixture(12);give_carried_items();races[1].flags1=RF1_UNIQUE;
    remote.feat[10][10]=FEAT_DEEP_WATER;
    cave_vertical_monster_end_action(1);cave_vertical_monster_end_action(1);
    assert(impact_calls==1&&impact_levels==1&&impact_feature==FEAT_DEEP_WATER);
    assert(last_damage==6&&last_damage>0&&damage_calls==1&&remote.monsters[1].hp==24);
    assert(!monster->r_idx&&mon_cnt==0&&remote.mon_cnt==1&&races[1].cur_num==1);
    assert(o_cnt==0&&remote.o_cnt==2);
    assert(remote_objects[1].number+remote_objects[2].number==11);
    int deep_impact=last_damage;

    monster=reset_fixture(12);remote.feat[10][10]=FEAT_FLOOR;
    cave_vertical_monster_end_action(1);
    assert(impact_calls==1&&impact_levels==1&&impact_feature==FEAT_FLOOR);
    assert(last_damage==12&&last_damage>deep_impact&&remote.monsters[1].hp==18);
    monster=reset_fixture(12);remote.feat[10][10]=FEAT_WATER;
    cave_vertical_monster_end_action(1);
    assert(impact_calls==1&&impact_feature==FEAT_WATER&&last_damage==9);
    assert(remote.monsters[1].hp==21&&remote.mon_cnt==1&&!legacy_deletions);

    monster=reset_fixture(12);remote.feat[10][10]=FEAT_DEEP_WATER;monster->hp=5;
    races[1].flags1=RF1_UNIQUE;
    cave_vertical_monster_end_action(1);cave_vertical_monster_end_action(1);
    assert(impact_calls==1&&damage_calls==1&&deaths==1&&!remote.mon_cnt);
    assert(!monster->r_idx&&mon_cnt==0&&races[1].cur_num==0); /* water remains dangerous */

    monster=reset_fixture(12);vertical.portals[0].peer_depth=3;
    assert(!cave_vertical_monster_fall(1));
    assert(monster->hp==30&&!impact_calls&&!damage_calls); /* no nonresident two-floor teleport */
    puts("vertical actors and real legacy m_fall chain: deferred ownership, holes and currents PASS");
    return 0;
}

/* Production-source fixture. Unused command/UI sections are discarded by the
 * linker; ray tests and the shared-clock actor process are the actual code. */
#include "../src/cave/cave-vertical-combat.c"
#include <assert.h>

static player_type player;
static player_other options;
static monster_race races[3];
static monster_lore lore[3];
static maxima limits;
static monster_type active_monsters[MAX_MONSTERS];
static object_type active_objects[8], remote_objects[8];
static level_image remote;
static u16b seen[32][256];
static byte terrain[32][MAX_DUNGEON_WID];
static s16b occupancy[32][MAX_DUNGEON_WID];
static s16b lighting[32][MAX_DUNGEON_WID];
_Thread_local player_type* p_ptr = &player;
_Thread_local player_other* op_ptr = &options;
_Thread_local monster_race* r_info = races;
_Thread_local monster_lore* l_list = lore;
_Thread_local u16b (*cave_info)[256] = seen;
_Thread_local byte (*cave_feat)[MAX_DUNGEON_WID] = terrain;
_Thread_local s16b (*cave_m_idx)[MAX_DUNGEON_WID] = occupancy;
_Thread_local s16b (*cave_light)[MAX_DUNGEON_WID] = lighting;
_Thread_local s32b turn;
maxima* z_info=&limits;
_Thread_local monster_type* mon_list=active_monsters;
_Thread_local object_type* o_list=active_objects;
_Thread_local s16b mon_max,mon_cnt,o_max,o_cnt,num_repro;
_Thread_local bool shimmer_monsters;
const byte extract_energy[8] = {0,5,10,15,20,25,30,35};
byte spell_info_RF4[32][3];
static bool aperture_open, clear_ray;
static int hurt, rolls;

int distance(int y,int x,int yy,int xx)
{ return MAX(ABS(y-yy),ABS(x-xx)); }
byte projectable(int y,int x,int yy,int xx,u32b flags)
{ (void)y;(void)x;(void)yy;(void)xx;(void)flags;return clear_ray?PROJECT_CLEAR:PROJECT_NO; }
bool level_cache_vertical_aperture(int y,int x,int* depth,int* py,int* px)
{
    if(!aperture_open||y!=10||x!=10)return false;
    *depth=2;*py=10;*px=10;return true;
}
const struct level_image* level_cache_floor(int depth)
{ return depth==2?&remote:NULL; }
struct level_image* level_cache_mutable_floor(int depth)
{ return depth==2?&remote:NULL; }
int elf_bane_bonus(monster_type* m) { (void)m;return 0; }
int dwarf_bane_bonus(monster_type* m) { (void)m;return 0; }
int edain_bane_bonus(monster_type* m) { (void)m;return 0; }
int unique_bane_bonus(monster_type* m) { (void)m;return 0; }
bool monster_race_is_vala(int index) { (void)index;return false; }
int monster_skill(monster_type* m,int skill) { (void)m;(void)skill;return 10; }
int total_player_evasion(monster_type* m,bool ranged)
{ (void)m;(void)ranged;return 10; }
int hit_roll(int a,int e,const monster_type* m,const monster_type* n,bool display)
{ (void)a;(void)e;(void)m;(void)n;(void)display;rolls++;return 1; }
int crit_bonus(int h,int w,const monster_race* r,int s,bool t,monster_type* m,const object_type* o)
{ (void)h;(void)w;(void)r;(void)s;(void)t;(void)m;(void)o;return 0; }
int get_sides(int attack) { (void)attack;return 10; }
int damroll(int dice,int sides) { return dice*sides; }
int protection_roll(int typ,bool melee) { (void)typ;(void)melee;return 3; }
void update_combat_rolls2(int dd,int ds,int d,int pd,int ps,int p,int percent,int type,bool melee)
{ (void)dd;(void)ds;(void)d;(void)pd;(void)ps;(void)p;(void)percent;(void)type;(void)melee; }
void disturb(int a,int b) { (void)a;(void)b; }
void msg_print(cptr message) { (void)message; }
void monster_desc(char* desc,size_t size,const monster_type* m,int mode)
{ (void)m;(void)mode;snprintf(desc,size,"test archer"); }
void take_hit(int damage,cptr killer) { (void)killer;hurt+=damage; }
int skill_check(monster_type* a,int s,int d,monster_type* b)
{ (void)a;(void)s;(void)d;(void)b;return 0; }
bool set_slow(int value) { (void)value;return true; }
s16b mon_pop(void)
{
    for(int i=1;i<MAX_MONSTERS;i++)if(!mon_list[i].r_idx)
    {mon_cnt++;if(i>=mon_max)mon_max=i+1;return i;}
    return 0;
}
s16b o_pop(void)
{
    for(int i=1;i<limits.o_max;i++)if(!o_list[i].k_idx)
    {o_cnt++;if(i>=o_max)o_max=i+1;return i;}
    assert(!"object preflight failed");return 0;
}
void update_mon(int index,bool full) { (void)index;(void)full; }
void lite_spot(int y,int x) { (void)y;(void)x; }

static monster_type* reset_fixture(void)
{
    memset(&player,0,sizeof(player));memset(races,0,sizeof(races));
    memset(&options,0,sizeof(options));
    memset(&remote,0,sizeof(remote));memset(seen,0,sizeof(seen));
    memset(terrain,0,sizeof(terrain));memset(occupancy,0,sizeof(occupancy));
    memset(spell_info_RF4,0,sizeof(spell_info_RF4));
    memset(active_monsters,0,sizeof(active_monsters));
    memset(active_objects,0,sizeof(active_objects));
    memset(remote_objects,0,sizeof(remote_objects));
    limits.o_max=8;mon_max=o_max=1;mon_cnt=o_cnt=num_repro=0;
    player.cur_map_hgt=player.cur_map_wid=32;player.py=player.px=10;player.depth=1;
    remote.hgt=remote.wid=32;remote.depth=2;remote.mon_max=2;remote.mon_cnt=1;
    remote.objects=remote_objects;remote.o_max=1;
    remote.monsters[1].r_idx=1;remote.monsters[1].hp=20;
    remote.monsters[1].fy=10;remote.monsters[1].fx=12;
    remote.monsters[1].mspeed=2;remote.monsters[1].alertness=ALERTNESS_ALERT;
    races[1].speed=2;
    remote.monsters[1].mana=10;remote.monsters_at[10][12]=1;
    remote.light[10][12]=1;seen[10][10]=CAVE_SEEN;
    aperture_open=clear_ray=true;hurt=rolls=0;turn=100;
    cave_vertical_combat_reset();return &remote.monsters[1];
}

int main(void)
{
    monster_type* m=reset_fixture();vertical_line line;
    assert(opening_line(10,10,&line)); /* standing on the opening */
    assert(visible_target(&remote,&line,m));
    assert(cave_vertical_combat_available(10,10));
    remote.info[10][11]=CAVE_WALL;
    assert(!cached_line(&remote,10,10,10,12));
    assert(!cave_vertical_combat_available(10,10));
    remote.info[10][11]=0;remote.monsters_at[10][11]=2;
    assert(!cached_line(&remote,10,10,10,12));
    remote.monsters_at[10][11]=0;remote.monsters_at[10][10]=2;
    assert(!cached_line(&remote,10,10,10,12));
    remote.monsters_at[10][10]=0;
    remote.info[10][11]=CAVE_WALL;
    assert(!cached_line(&remote,10,10,11,11)); /* closed corner */
    remote.info[10][11]=0;remote.light[10][12]=0;
    assert(!visible_target(&remote,&line,m));
    remote.light[10][12]=1;races[1].flags2=RF2_INVISIBLE;
    assert(!visible_target(&remote,&line,m));player.see_inv=1;
    assert(visible_target(&remote,&line,m));player.blind=1;
    assert(!visible_target(&remote,&line,m));

    m=reset_fixture();races[1].flags4=RF4_ARROW1;
    spell_info_RF4[0][COL_SPELL_MANA_COST]=2;
    cave_vertical_combat_process();turn+=9;cave_vertical_combat_process();
    assert(m->energy==90&&hurt==0&&rolls==0);
    turn++;cave_vertical_combat_process();
    assert(m->energy==0&&hurt==7&&rolls==1&&m->mana==8);
    cave_vertical_combat_process();assert(hurt==7&&rolls==1); /* no double tick */
    aperture_open=false;turn+=10;cave_vertical_combat_process();assert(hurt==7);
    aperture_open=true;player.blind=1;turn+=10;cave_vertical_combat_process();
    assert(hurt==14&&rolls==2); /* blindness never freezes enemy retaliation */
    remote.info[10][11]=CAVE_WALL;turn+=10;cave_vertical_combat_process();
    assert(hurt==14&&rolls==2);

    m=reset_fixture();races[1].flags4=RF4_ARROW1;
    races[1].speed=m->mspeed=7; /* fastest ordinary speed gets three actions */
    cave_vertical_combat_process();turn+=10;cave_vertical_combat_process();
    assert(rolls==3&&hurt==21&&m->energy==50);

    m=reset_fixture();cave_vertical_combat_process();turn+=10;
    cave_vertical_combat_process();assert(m->fx==11&&remote.monsters_at[10][11]==1);
    assert(!remote.monsters_at[10][12]);

    m=reset_fixture();races[1].flags1=RF1_UNIQUE;races[1].flags2=RF2_FLYING;
    races[1].cur_num=1;m->fy=m->fx=10;remote.monsters_at[10][12]=0;
    remote.monsters_at[10][10]=1;m->hold_o_idx=1;
    remote.o_max=3;remote.o_cnt=2;remote_objects[1].k_idx=3;
    remote_objects[1].number=2;remote_objects[1].held_m_idx=1;
    remote_objects[1].next_o_idx=2;remote_objects[2].k_idx=4;
    remote_objects[2].number=5;remote_objects[2].held_m_idx=1;
    assert(opening_line(10,10,&line));
    mon_max=MAX_MONSTERS;
    for(int i=1;i<MAX_MONSTERS;i++)active_monsters[i].r_idx=2;
    assert(!fly_through(&remote,1,&line));
    assert(m->r_idx==1&&remote.o_cnt==2&&races[1].cur_num==1);
    memset(active_monsters,0,sizeof(active_monsters));mon_max=1;
    limits.o_max=2;o_max=2;active_objects[1].k_idx=2;
    assert(!fly_through(&remote,1,&line)); /* insufficient carried-object space */
    assert(m->r_idx==1&&remote.o_cnt==2&&mon_cnt==0);
    limits.o_max=8;memset(active_objects,0,sizeof(active_objects));o_max=1;
    assert(fly_through(&remote,1,&line));
    assert(!m->r_idx&&remote.mon_cnt==0&&!remote.monsters_at[10][10]);
    assert(remote.o_cnt==0&&!remote_objects[1].k_idx&&!remote_objects[2].k_idx);
    assert(mon_cnt==1&&o_cnt==2&&races[1].cur_num==1);
    assert(active_monsters[1].r_idx==1&&active_monsters[1].hp==20);
    assert(active_objects[1].held_m_idx==1&&active_objects[2].held_m_idx==1);
    assert(active_objects[1].number+active_objects[2].number==7);
    assert(active_monsters[1].hold_o_idx==2&&active_objects[2].next_o_idx==1);
    m=reset_fixture();races[1].flags2=RF2_FLYING;m->fy=m->fx=10;
    remote.monsters_at[10][12]=0;remote.monsters_at[10][10]=1;
    assert(opening_line(10,10,&line));assert(fly_through(&remote,1,&line));
    assert(races[1].cur_num==1); /* ordinary race ledger is active-floor only */
    puts("vertical combat geometry, visibility and shared-clock retaliation: PASS");
    return 0;
}

#include "angband.h"
#include "externs.h"
#include "cave/cave-environment.h"
#include "cave/cave-events.h"
#include "cave/cave-bridge.h"
#include "cave/cave-fixtures.h"
#include "cave/cave-water-flow.h"
#include "level-generation/level-generation-terrain-access.h"
#include "level-generation/level-generation-terrain-vaults.h"
#include "fs/path.h"
#include "log/log.h"
#include <stdio.h>

/* The ecology owns a world clock and random stream. Rendering, opening a menu,
 * and the order in which monsters notice an event cannot advance it. */
static environment_cell cells[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static environment_source sources[ENV_SOURCES_MAX];
static environment_state state;
static s16b next_heat[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte shadow[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte before[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte after[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static int queue[MAX_DUNGEON_HGT * MAX_DUNGEON_WID];
static const int dy4[4] = {-1, 0, 1, 0}, dx4[4] = {0, 1, 0, -1};
static bool changing;
static int pending_count;
static struct {
    int pulse, water_step, lava_step, acid_step, crack_step, warning;
    int growth_cap, changes;
} config = {100, 400, 700, 600, 1200, 50, 24, 4};
static bool config_loaded;

static int speed_multiplier(byte speed)
{
    return speed == ENVIRONMENT_SPEED_SLOW ? 1
        : speed == ENVIRONMENT_SPEED_FAST ? 4 : 2;
}

static int scaled_interval(int ticks)
{
    int rate = speed_multiplier(op_ptr->environment_speed);
    return MAX(10, (ticks + rate - 1) / rate);
}

void cave_environment_set_speed(byte speed)
{
    int old_rate = speed_multiplier(op_ptr->environment_speed);
    if (speed > ENVIRONMENT_SPEED_MAX) speed = ENVIRONMENT_SPEED_NORMAL;
    op_ptr->environment_speed = speed;
    int rate = speed_multiplier(speed);
    if (!state.ready || rate == old_rate) return;
    /* Apply a menu change to current sources too, without advancing geology
     * or shortening warnings that have already been given. */
    for (int i = 0; i < state.source_count; i++)
        if (sources[i].next_turn > turn)
            sources[i].next_turn = turn + MIN(100000, MAX(10,
                ((sources[i].next_turn - turn) * old_rate + rate - 1) / rate));
}

static void load_config(void)
{
    if (config_loaded) return;
    config_loaded = true;
    char path[1024], line[160], key[64], extra;
    if (!ANGBAND_DIR_EDIT || !path_build(path, sizeof(path), ANGBAND_DIR_EDIT,
            "dungeon-dynamics.txt")) return;
    FILE* file = fopen(path, "r");
    if (!file) return;
    while (fgets(line, sizeof(line), file)) {
        int value;
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        if (sscanf(line, "%63[^:]:%d %c", key, &value, &extra) != 2) {
            log_warn("Invalid dungeon dynamics record: %s", line); continue;
        }
        int* target = NULL, low = 10, high = 100000;
        if (!strcmp(key, "pulse")) target = &config.pulse;
        else if (!strcmp(key, "water_step")) target = &config.water_step;
        else if (!strcmp(key, "lava_step")) target = &config.lava_step;
        else if (!strcmp(key, "acid_step")) target = &config.acid_step;
        else if (!strcmp(key, "crack_step")) target = &config.crack_step;
        else if (!strcmp(key, "warning")) { target = &config.warning; low = 30; }
        else if (!strcmp(key, "growth_cap")) { target = &config.growth_cap; low = 1; high = 64; }
        else if (!strcmp(key, "changes")) { target = &config.changes; low = 1; high = 16; }
        if (target && value >= low && value <= high) *target = value;
        else log_warn("Unknown or out-of-range dungeon dynamics setting: %s", key);
    }
    fclose(file);
}

static unsigned random_below(unsigned n)
{
    state.random ^= state.random << 13;
    state.random ^= state.random >> 17;
    state.random ^= state.random << 5;
    return n ? state.random % n : 0;
}

static int liquid(int feature)
{
    feature = cave_bridge_underlay(feature);
    if (feature == FEAT_DEEP_WATER || FEAT_IS_ICE(feature)) return FEAT_WATER;
    return feature == FEAT_WATER || feature == FEAT_LAVA || feature == FEAT_POISON
        || feature == FEAT_CHASM ? feature : 0;
}

static bool rock(int feature)
{
    return feature == FEAT_QUARTZ || (feature >= FEAT_WALL_EXTRA
        && feature <= FEAT_WALL_SOLID);
}

static bool anchor(int y, int x)
{
    int f = cave_feat[y][x];
    return f == FEAT_WALL_PERM || f == FEAT_NONE || f == FEAT_GLYPH
        || f == FEAT_SUNLIGHT || cave_stair_bold(y, x) || cave_forge_bold(y, x)
        || f == FEAT_WARDED || f == FEAT_WARDED2 || f == FEAT_WARDED3;
}

static bool critical_object(int y, int x)
{
    for (object_type* obj = get_first_object(y, x); obj; obj = get_next_object(obj))
        if (artefact_p(obj) || obj->tval == TV_NOTE) return true;
    return false;
}

static int origin_heat(int y, int x)
{
    int f = cave_bridge_underlay(cave_feat[y][x]);
    if (f == FEAT_LAVA) return 12;
    if (FEAT_IS_ICE(f) && !(cells[y][x].flags & ENV_FLOOR_ICE)) return -8;
    big_cave_type_t kind = level_partition_big_cave_type_for_point(y, x);
    if (kind == BIG_CAVE_ICE && (cells[y][x].flags & ENV_NATURAL)) return -6;
    if (kind == BIG_CAVE_FIRE && (cells[y][x].flags & ENV_NATURAL)) return 6;
    return 0;
}

void cave_environment_reset(void)
{
    memset(cells, 0, sizeof(cells));
    memset(sources, 0, sizeof(sources));
    memset(&state, 0, sizeof(state));
    changing = false;
    pending_count = 0;
    cave_events_reset();
}

static bool floor_bridge(int y, int x, bool* vertical)
{
    if (cave_feat[y][x] != FEAT_FLOOR || !(cave_info[y][x] & CAVE_CHASM_AREA)) return false;
    bool sides = cave_feat[y][x-1] == FEAT_CHASM && cave_feat[y][x+1] == FEAT_CHASM;
    bool ends = cave_feat[y-1][x] == FEAT_CHASM && cave_feat[y+1][x] == FEAT_CHASM;
    *vertical = sides;
    return sides || ends;
}

void cave_environment_seed(void)
{
    cave_environment_reset();
    load_config();
    state.random = 0x9e3779b9U ^ (u32b)turn ^ ((u32b)p_ptr->depth << 16);
    if (!state.random) state.random = 1;
    state.last_turn = turn;
    state.mineral_budget = 8;
    for (int y = 0; y < p_ptr->cur_map_hgt; y++) for (int x = 0; x < p_ptr->cur_map_wid; x++) {
        environment_cell* c = &cells[y][x];
        int f = cave_feat[y][x];
        c->base_feat = c->known_feat = f;
        c->integrity = 100;
        level_partition_kind part = level_partition_kind_for_point(y, x);
        if (cave_natural[y][x] || part == LEVEL_PART_CAVEY || part == LEVEL_PART_RUINED
            || part == LEVEL_PART_BIG_CAVE || part == LEVEL_PART_CHASM) c->flags |= ENV_NATURAL;
        if (!in_bounds_fully(y, x) || anchor(y, x)
            || (cave_info[y][x] & (CAVE_G_VAULT | CAVE_MORGOTH_TUNNEL))
            || ((cave_info[y][x] & CAVE_ICKY) && !terrain_vault_policy_at(y, x))
            || cave_fixture_at(y, x)) c->flags |= ENV_PROTECTED;
        bool vertical = false;
        if (in_bounds_fully(y, x) && (FEAT_IS_BRIDGE(f) || floor_bridge(y, x, &vertical))) {
            c->flags |= ENV_BRIDGE;
            c->bridge_feat = f;
            c->underlay = FEAT_IS_BRIDGE(f) ? cave_bridge_underlay(f) : FEAT_CHASM;
            c->material = (f == FEAT_FLOOR || c->underlay == FEAT_CHASM
                || c->underlay == FEAT_LAVA || c->underlay == FEAT_POISON
                || FEAT_IS_ICE(c->underlay)) ? ENV_BRIDGE_STONE : ENV_BRIDGE_WOOD;
        }
        c->heat = origin_heat(y, x);
        c->known_underlay = c->flags & ENV_BRIDGE ? c->underlay : cave_bridge_underlay(f);
        c->known_material = c->material;
    }
    /* Exact terrain components become bounded reservoirs. This works for old
     * saves too; no generation-only pointers survive into the simulation. */
    for (int y = 1; y < p_ptr->cur_map_hgt-1; y++) for (int x = 1; x < p_ptr->cur_map_wid-1; x++) {
        int f = liquid(cave_feat[y][x]);
        if (!f || cells[y][x].owner || (cells[y][x].flags & ENV_PROTECTED)
            || state.source_count >= ENV_SOURCES_MAX-3) continue;
        int id = ++state.source_count, head = 0, tail = 1;
        queue[0] = y * MAX_DUNGEON_WID + x;
        cells[y][x].owner = id;
        while (head < tail) {
            int pos = queue[head++], cy = pos/MAX_DUNGEON_WID, cx = pos%MAX_DUNGEON_WID;
            for (int d = 0; d < 4; d++) {
                int ny = cy+dy4[d], nx = cx+dx4[d];
                if (!in_bounds_fully(ny,nx) || cells[ny][nx].owner
                    || (cells[ny][nx].flags & ENV_PROTECTED) || liquid(cave_feat[ny][nx]) != f) continue;
                cells[ny][nx].owner = id;
                queue[tail++] = ny*MAX_DUNGEON_WID+nx;
            }
        }
        environment_source* s = &sources[id-1];
        s->y=y; s->x=x; s->feature=f;
        s->capacity = MIN(config.growth_cap, MAX(2, tail/8));
        s->next_turn = turn + scaled_interval(200 + random_below(500));
        s->phase = random_below(12);
    }
    /* Buried sources are chosen with the accepted geology, not rolled under
     * the player later. Only matching elemental partitions host new vents. */
    for (int n = 0; n < 3 && state.source_count < ENV_SOURCES_MAX; n++) {
        int sy = 0, sx = 0, material = 0, candidates = 0;
        for (int y = 2; y < p_ptr->cur_map_hgt-2; y++) for (int x = 2; x < p_ptr->cur_map_wid-2; x++) {
            environment_cell* c = &cells[y][x];
            big_cave_type_t kind = level_partition_big_cave_type_for_point(y,x);
            int f = kind == BIG_CAVE_FIRE ? FEAT_LAVA : kind == BIG_CAVE_POIS ? FEAT_POISON : 0;
            if (!f || c->owner || (c->flags & ENV_PROTECTED) || !(c->flags & ENV_NATURAL)
                || cave_feat[y][x] != FEAT_FLOOR || cave_o_idx[y][x] || cave_m_idx[y][x]
                || distance(y,x,p_ptr->py,p_ptr->px) < 7) continue;
            int walls = 0;
            for (int d=0;d<4;d++) walls += rock(cave_feat[y+dy4[d]][x+dx4[d]]);
            if (walls >= 2 && random_below(++candidates)==0) { sy=y; sx=x; material=f; }
        }
        if (!candidates) break;
        int id = ++state.source_count;
        cells[sy][sx].owner=id;
        sources[id-1]=(environment_source){sy,sx,material,6,0,12,
            turn+scaled_interval(1500+(int)random_below(3000))};
    }
    state.ready = true;
}

void cave_environment_observe(int y, int x)
{
    if (state.ready && in_bounds(y,x) && (cave_info[y][x] & CAVE_SEEN)) {
        environment_cell* c = &cells[y][x];
        int f = cave_feat[y][x];
        c->known_feat = f;
        c->known_underlay = (c->flags & ENV_BRIDGE) && c->integrity
            && (FEAT_IS_BRIDGE(f) || f == c->bridge_feat) ? c->underlay : cave_bridge_underlay(f);
        c->known_material = c->material;
    }
}

int cave_environment_known_feature(int y, int x)
{
    if (!in_bounds(y,x)) return FEAT_NONE;
    if (state.ready && !(cave_info[y][x] & CAVE_SEEN) && (cave_info[y][x] & CAVE_MARK))
        return cells[y][x].known_feat;
    return cave_feat[y][x];
}

int cave_environment_display_underlay(int y, int x)
{
    int feature = cave_environment_known_feature(y,x);
    if (state.ready && in_bounds(y,x) && !(cave_info[y][x]&CAVE_SEEN)
        && (cave_info[y][x]&CAVE_MARK)) return cells[y][x].known_underlay;
    if (state.ready && in_bounds(y,x) && (cave_info[y][x]&CAVE_SEEN)
        && (cells[y][x].flags&ENV_BRIDGE) && cells[y][x].integrity
        && (FEAT_IS_BRIDGE(feature) || feature==cells[y][x].bridge_feat))
        return cells[y][x].underlay;
    return cave_bridge_underlay(feature);
}

void cave_environment_changed(int y, int x, int old_feat, int new_feat)
{
    cave_events_terrain_changed();
    if (!state.ready || !in_bounds(y,x) || old_feat == new_feat) return;
    environment_cell* c = &cells[y][x];
    if (cave_info[y][x] & CAVE_SEEN) c->known_feat = new_feat;
    if (!changing) {
        if (c->pending_feat && pending_count) pending_count--;
        c->pending_feat=0; c->due=0;
        c->work=0;
        if (old_feat == FEAT_QUARTZ) c->flags |= ENV_MINERAL_SPENT;
        if (c->flags & ENV_FLOOR_ICE) {
            if (!FEAT_IS_ICE(new_feat)) c->flags &= ~ENV_FLOOR_ICE;
        }
        if ((c->flags & ENV_BRIDGE) && !FEAT_IS_BRIDGE(new_feat) && new_feat != c->bridge_feat) {
            c->integrity=0; c->work=0;
            if (liquid(new_feat)) c->underlay=new_feat;
        }
        if (rock(old_feat) && !rock(new_feat)) cave_event_emit(CAVE_EVENT_DIG,y,x,12);
        else if (FEAT_IS_ICE(old_feat) && !FEAT_IS_ICE(new_feat)) cave_event_emit(CAVE_EVENT_THAW,y,x,8);
    }
}

static bool walkable(int y,int x,const byte (*map)[MAX_DUNGEON_WID])
{
    return terrain_generation_walkable(y,x,map) && map[y][x] != FEAT_DEEP_WATER
        && map[y][x] != FEAT_TRAP_PIT && map[y][x] != FEAT_TRAP_SPIKED_PIT;
}

static void reachable(byte result[MAX_DUNGEON_HGT][MAX_DUNGEON_WID],
    const byte (*map)[MAX_DUNGEON_WID])
{
    memset(result,0,sizeof(before));
    int head=0,tail=0,sy=p_ptr->py,sx=p_ptr->px;
    if (!in_bounds_fully(sy,sx) || !walkable(sy,sx,map)) return;
    queue[tail++]=sy*MAX_DUNGEON_WID+sx; result[sy][sx]=1;
    while(head<tail) {
        int p=queue[head++],y=p/MAX_DUNGEON_WID,x=p%MAX_DUNGEON_WID;
        for(int dy=-1;dy<=1;dy++) for(int dx=-1;dx<=1;dx++) {
            if(!dy&&!dx) continue;
            int ny=y+dy,nx=x+dx;
            if(!walkable(ny,nx,map)) {
                if(!p_ptr->active_ability[S_EVN][EVN_LEAPING]
                    || !terrain_generation_jump(y,x,dy,dx,map)) continue;
                ny+=dy;nx+=dx;
            }
            if(!in_bounds_fully(ny,nx)||result[ny][nx]) continue;
            result[ny][nx]=1;queue[tail++]=ny*MAX_DUNGEON_WID+nx;
        }
    }
}

static bool preserves_routes(int y,int x,int feature)
{
    if (walkable(y,x,(const byte (*)[MAX_DUNGEON_WID])cave_feat)
        && feature != FEAT_FLOOR && feature != FEAT_WATER && feature != FEAT_ICE
        && !FEAT_IS_BRIDGE(feature)) {
        memcpy(shadow,cave_feat,sizeof(shadow));
        reachable(before,(const byte (*)[MAX_DUNGEON_WID])cave_feat);
        shadow[y][x]=feature;
        reachable(after,(const byte (*)[MAX_DUNGEON_WID])shadow);
        for(int yy=1;yy<p_ptr->cur_map_hgt-1;yy++) for(int xx=1;xx<p_ptr->cur_map_wid-1;xx++)
            if(before[yy][xx] && (cave_stair_bold(yy,xx)||cave_forge_bold(yy,xx)
                || critical_object(yy,xx)) && !after[yy][xx]) return false;
        /* A closing passage cannot strand a previously connected creature. */
        for(int i=1;i<mon_max;i++) if(mon_list[i].r_idx) {
            int my=mon_list[i].fy,mx=mon_list[i].fx;
            if(before[my][mx]&&!after[my][mx]) return false;
        }
    }
    return true;
}

static bool commit(int y,int x,int feature,int event)
{
    environment_cell* c=&cells[y][x];
    if((c->flags&ENV_PROTECTED)||critical_object(y,x)||cave_m_idx[y][x]
        || !preserves_routes(y,x,feature)) return false;
    int old=cave_feat[y][x];
    changing=true;
    cave_set_feat(y,x,feature);
    changing=false;
    if(cave_info[y][x]&CAVE_SEEN)c->known_feat=feature;
    if (c->pending_feat && pending_count) pending_count--;
    c->pending_feat=0;c->due=0;
    p_ptr->update |= PU_UPDATE_VIEW | PU_MONSTERS;
    p_ptr->redraw |= PR_MAP;
    if(event)cave_event_emit(event,y,x,event==CAVE_EVENT_COLLAPSE?22:12);
    if(old==FEAT_QUARTZ)c->flags|=ENV_MINERAL_SPENT;
    return true;
}

static bool propose(int y,int x,int feature,int event)
{
    environment_cell* c=&cells[y][x];
    if(pending_count>=16 || c->pending_feat || feature==cave_feat[y][x] || (c->flags&ENV_PROTECTED)
        || critical_object(y,x))return false;
    if(!preserves_routes(y,x,feature))return false;
    c->pending_feat=feature;c->due=turn+config.warning;pending_count++;
    cave_event_emit(event,y,x,16);
    if(cave_info[y][x]&CAVE_SEEN) lite_spot(y,x);
    return true;
}

int cave_environment_pending_hazard(int y,int x)
{
    return state.ready&&in_bounds_fully(y,x)?cells[y][x].pending_feat:0;
}

int cave_environment_thaw_feature(int y,int x,int fallback)
{
    return state.ready&&in_bounds_fully(y,x)&&(cells[y][x].flags&ENV_FLOOR_ICE)
        ?cells[y][x].base_feat:fallback;
}

static void update_heat(void)
{
    for(int y=1;y<p_ptr->cur_map_hgt-1;y++)for(int x=1;x<p_ptr->cur_map_wid-1;x++) {
        int heat=origin_heat(y,x), strongest=0;
        if(cave_feat[y][x]==FEAT_WALL_PERM){next_heat[y][x]=0;continue;}
        for(int d=0;d<4;d++) {
            int n=cells[y+dy4[d]][x+dx4[d]].heat;
            int resistance=rock(cave_feat[y][x])?4:1;
            n=n>0?MAX(0,n-resistance):MIN(0,n+resistance);
            strongest+=n;
        }
        int incoming=strongest/4;
        if(!heat)heat=incoming;
        else if((heat<0&&incoming>0)||(heat>0&&incoming<0))heat+=incoming;
        next_heat[y][x]=MAX(-12,MIN(12,heat));
    }
    for(int y=1;y<p_ptr->cur_map_hgt-1;y++)for(int x=1;x<p_ptr->cur_map_wid-1;x++)
        cells[y][x].heat=next_heat[y][x];
}

void cave_environment_flood_bridge(int y,int x,int fluid,int force)
{
    if(!state.ready||!in_bounds_fully(y,x))return;
    environment_cell* c=&cells[y][x];
    if(!(c->flags&ENV_BRIDGE)||(c->flags&ENV_PROTECTED)||!c->integrity)return;
    if(c->underlay!=fluid)c->work=0;
    c->underlay=fluid;
    cave_environment_observe(y,x);
    int damage=force*(c->material==ENV_BRIDGE_WOOD?3:1);
    if(fluid==FEAT_LAVA)damage*=c->material==ENV_BRIDGE_WOOD?3:2;
    else if(fluid==FEAT_POISON)damage*=2;
    c->integrity=MAX(1,(int)c->integrity-damage);
    if(c->integrity<=25)propose(y,x,fluid,CAVE_EVENT_CRACK);
}

static void source_step(int id)
{
    environment_source* s=&sources[id-1];
    int interval=s->feature==FEAT_LAVA?config.lava_step:s->feature==FEAT_POISON?
        config.acid_step:s->feature==FEAT_CHASM?config.crack_step:config.water_step;
    if(turn<s->next_turn)return;
    s->next_turn=turn+scaled_interval(interval);
    if (s->phase==12) {
        environment_cell* origin=&cells[s->y][s->x];
        if (cave_feat[s->y][s->x]==FEAT_FLOOR && !cave_m_idx[s->y][s->x]
            && propose(s->y,s->x,s->feature,CAVE_EVENT_VENT)) {
            origin->flags|=ENV_ADDED_LIQUID;
            origin->base_feat=FEAT_FLOOR;s->used=1;s->phase=0;
        }
        return;
    }
    bool rising=s->phase++%12<6;
    s->phase%=12;
    if(s->feature==FEAT_CHASM)rising=true;
    int chosen_y=0,chosen_x=0,seen=0;
    for(int y=1;y<p_ptr->cur_map_hgt-1;y++)for(int x=1;x<p_ptr->cur_map_wid-1;x++) {
        environment_cell* c=&cells[y][x]; int f=cave_feat[y][x];
        if(c->flags&ENV_PROTECTED||c->pending_feat)continue;
        if(!rising) {
            if(c->owner!=id||!(c->flags&ENV_ADDED_LIQUID)||(c->flags&ENV_BRIDGE)
                || liquid(f)!=s->feature)continue;
        } else {
            if(c->owner && c->owner!=id)continue;
            bool contact=false;
            for(int d=0;d<4;d++) {
                int ny=y+dy4[d],nx=x+dx4[d];
                if(cells[ny][nx].owner==id && liquid(cave_feat[ny][nx])==s->feature
                    && !FEAT_IS_ICE(cave_feat[ny][nx])) contact=true;
            }
            if(!contact)continue;
            if((c->flags&ENV_BRIDGE)&&c->integrity) {
                cave_environment_flood_bridge(y,x,s->feature,4);continue;
            }
            if(rock(f)) {
                level_partition_kind part=level_partition_kind_for_point(y,x);
                if(part==LEVEL_PART_LABYRINTH)continue;
                int wear=s->feature==FEAT_POISON?16:s->feature==FEAT_LAVA?12:5;
                if(part==LEVEL_PART_RUINED)wear*=2;
                c->integrity=MAX(1,(int)c->integrity-wear);
                if(c->integrity<=25)propose(y,x,FEAT_RUBBLE,CAVE_EVENT_CRACK);
                continue;
            }
            if(s->used>=s->capacity || (f!=FEAT_FLOOR&&f!=FEAT_RUBBLE
                &&f!=FEAT_OPEN&&f!=FEAT_BROKEN))continue;
            if(s->feature==FEAT_CHASM && (!(c->flags&ENV_NATURAL)||p_ptr->depth>=MORGOTH_DEPTH))continue;
        }
        if(critical_object(y,x)||cave_m_idx[y][x])continue;
        if(random_below(++seen)==0){chosen_y=y;chosen_x=x;}
    }
    if(!seen)return;
    environment_cell* c=&cells[chosen_y][chosen_x];
    if(!rising) {
        int target=s->feature==FEAT_LAVA?FEAT_FLOOR:c->base_feat;
        if(commit(chosen_y,chosen_x,target,s->feature==FEAT_LAVA?CAVE_EVENT_VENT:CAVE_EVENT_FLOOD)) {
            c->flags&=~ENV_ADDED_LIQUID;
            if(s->used)s->used--;
        }
    } else if(propose(chosen_y,chosen_x,s->feature,
            s->feature==FEAT_CHASM?CAVE_EVENT_CRACK:s->feature==FEAT_LAVA?CAVE_EVENT_VENT:CAVE_EVENT_FLOOD)) {
        c->base_feat=cave_feat[chosen_y][chosen_x]==FEAT_RUBBLE?FEAT_FLOOR:cave_feat[chosen_y][chosen_x];
        c->owner=id;
        /* Reserve supply when scheduled, so simultaneous proposals cannot
         * borrow the same unit. Cancellation refunds it in the pulse. */
        c->flags|=ENV_ADDED_LIQUID;s->used++;
    }
}

void cave_environment_process(void)
{
    if(!state.ready||!character_dungeon||p_ptr->leaving||p_ptr->is_dead||cheat_timestop)return;
    if (turn <= state.last_turn) return;
    int pulse = scaled_interval(config.pulse);
    bool geology_due = turn/pulse > state.last_turn/pulse;
    state.last_turn = turn;
    int remaining=config.changes;
    /* Warnings use a finer clock than geology so they last their advertised
     * number of world turns even when the geology pulse is slower. */
    for(int y=1;y<p_ptr->cur_map_hgt-1;y++)for(int x=1;x<p_ptr->cur_map_wid-1;x++) {
        environment_cell* c=&cells[y][x];
        if(!c->pending_feat||turn<c->due||!remaining)continue;
        int f=c->pending_feat, event=(f==FEAT_RUBBLE||f==FEAT_CHASM)?CAVE_EVENT_COLLAPSE:
            f==FEAT_LAVA?CAVE_EVENT_VENT:CAVE_EVENT_FLOOD;
        if(commit(y,x,f,event)) {
            if(c->flags&ENV_BRIDGE){c->integrity=0;c->work=0;c->underlay=f;}
            remaining--;
        } else if(turn-c->due>300) {
            if((c->flags&ENV_ADDED_LIQUID)&&c->owner&&liquid(cave_feat[y][x])!=sources[c->owner-1].feature) {
                if(sources[c->owner-1].used)sources[c->owner-1].used--;
                c->flags&=~ENV_ADDED_LIQUID;
            }
            c->pending_feat=0;c->due=0;if(pending_count)pending_count--;
        }
    }
    if(!geology_due)return;
    if(p_ptr->depth<1 || (p_ptr->depth>MORGOTH_DEPTH&&p_ptr->depth!=UTUMNO_DEPTH))return;
    update_heat();
    for(int i=1;i<=state.source_count;i++)source_step(i);
    /* Reservoir sampling gives a bounded number of thermal/mineral changes,
     * independent of map size and scan direction. */
    int cy=0,cx=0,target=0,event=0,seen=0;
    for(int y=1;y<p_ptr->cur_map_hgt-1;y++)for(int x=1;x<p_ptr->cur_map_wid-1;x++) {
        environment_cell* c=&cells[y][x]; int f=cave_feat[y][x],to=0,ev=0;
        if(c->flags&ENV_PROTECTED || c->pending_feat || (c->flags&ENV_BRIDGE)
            || cave_m_idx[y][x] || critical_object(y,x))continue;
        if(f==FEAT_WATER&&c->heat<=-2){to=FEAT_ICE;ev=CAVE_EVENT_FREEZE;}
        else if(FEAT_IS_ICE(f)&&c->heat>=2){to=(c->flags&ENV_FLOOR_ICE)?c->base_feat:
            f==FEAT_ICE?FEAT_MELTING_ICE:FEAT_WATER;ev=CAVE_EVENT_THAW;}
        else if(f==FEAT_FLOOR&&c->heat<=-4&&(c->flags&ENV_NATURAL)) {
            bool wet=false;
            for(int d=0;d<4;d++)wet|=liquid(cave_feat[y+dy4[d]][x+dx4[d]])==FEAT_WATER;
            if(wet){to=FEAT_ICE;ev=CAVE_EVENT_FREEZE;}
        } else if(f==FEAT_LAVA) {
            bool water=false;
            for(int d=0;d<4;d++)water|=liquid(cave_feat[y+dy4[d]][x+dx4[d]])==FEAT_WATER;
            if(water){to=FEAT_FLOOR;ev=CAVE_EVENT_VENT;}
        } else if(rock(f)&&f!=FEAT_QUARTZ&&state.mineral_budget
            &&(c->flags&ENV_NATURAL)&&!(c->flags&ENV_MINERAL_SPENT)&&random_below(1500)==0) {
            for(int d=0;d<4;d++)if(cave_feat[y+dy4[d]][x+dx4[d]]==FEAT_QUARTZ){to=FEAT_QUARTZ;ev=CAVE_EVENT_CRACK;}
        }
        if(to&&random_below(++seen)==0){cy=y;cx=x;target=to;event=ev;}
    }
    if(target&&remaining) {
        environment_cell* c=&cells[cy][cx];int old=cave_feat[cy][cx];
        if(commit(cy,cx,target,event)) {
            if(old==FEAT_FLOOR&&target==FEAT_ICE){c->flags|=ENV_FLOOR_ICE;c->base_feat=FEAT_FLOOR;}
            if(!FEAT_IS_ICE(target))c->flags&=~ENV_FLOOR_ICE;
            if(target==FEAT_QUARTZ){c->flags|=ENV_MINERAL_SPENT;state.mineral_budget--;}
            if(old==FEAT_LAVA)c->flags|=ENV_DEPOSIT;
        }
    }
}

bool cave_environment_job_safe(int y,int x)
{
    if(!state.ready||!in_bounds_fully(y,x)||(cells[y][x].flags&ENV_PROTECTED)
        || cells[y][x].pending_feat)return false;
    for(int d=0;d<4;d++)if(cells[y+dy4[d]][x+dx4[d]].pending_feat)return false;
    return true;
}

static bool bank(int y,int x)
{
    if(!in_bounds_fully(y,x)||!cave_environment_job_safe(y,x))return false;
    int f=cave_feat[y][x];
    return f==FEAT_FLOOR||f==FEAT_OPEN||f==FEAT_BROKEN||FEAT_IS_BRIDGE(f);
}

/* Historical spans can be repaired in sections, but their original chain
 * must still connect two real banks. An eroded island is not a foundation. */
static bool repair_axis(int y, int x, int axis)
{
    for (int sign = -1; sign <= 1; sign += 2) {
        bool found = false;
        for (int n = 1; n <= 24; n++) {
            int ny = y + axis * sign * n, nx = x + (1-axis) * sign * n;
            if (!in_bounds_fully(ny,nx) || !cave_environment_job_safe(ny,nx)) break;
            if (!(cells[ny][nx].flags & ENV_BRIDGE)) { found = bank(ny,nx); break; }
        }
        if (!found) return false;
    }
    return true;
}

bool cave_environment_bridge_job_at(int y,int x,environment_bridge_job* job)
{
    if(!job||!cave_environment_job_safe(y,x))return false;
    environment_cell* c=&cells[y][x];int f=cave_feat[y][x];
    if((c->flags&ENV_BRIDGE)&&c->integrity<100) {
        if(!c->integrity&&(!liquid(f)||cave_m_idx[y][x]))return false;
        int axis = FEAT_IS_BRIDGE(c->bridge_feat) ? cave_bridge_vertical(c->bridge_feat) : 0;
        if (!repair_axis(y,x,axis)) {
            if (FEAT_IS_BRIDGE(c->bridge_feat) || !repair_axis(y,x,1-axis)) return false;
            axis=1-axis;
        }
        int material = c->material;
        if (c->underlay == FEAT_CHASM || c->underlay == FEAT_LAVA
            || c->underlay == FEAT_POISON)
            material=ENV_BRIDGE_STONE;
        /* Restore even historical floor-over-chasm crossings as encoded
         * bridge features. The renderer needs that identity to draw the
         * repaired deck instead of leaving a plain floor tile. */
        int feature = cave_bridge_feature(c->underlay,axis != 0);
        if (!feature) return false;
        *job=(environment_bridge_job){y,x,feature,material,c->integrity,true};return true;
    }
    if(!liquid(f)||FEAT_IS_ICE(f)||FEAT_IS_BRIDGE(f)||cave_m_idx[y][x]||critical_object(y,x))return false;
    for(int axis=0;axis<2;axis++)for(int sign=-1;sign<=1;sign+=2) {
        int dy=axis*sign,dx=(1-axis)*sign;
        if(!bank(y-dy,x-dx))continue;
        int length=1;
        while(length<=2&&in_bounds_fully(y+dy*length,x+dx*length)
            &&cave_feat[y+dy*length][x+dx*length]==f
            &&cave_environment_job_safe(y+dy*length,x+dx*length))length++;
        if(length>2||!bank(y+dy*length,x+dx*length))continue;
        int material=(f==FEAT_CHASM||f==FEAT_LAVA||f==FEAT_POISON)
            ? ENV_BRIDGE_STONE : ENV_BRIDGE_WOOD;
        *job=(environment_bridge_job){y,x,cave_bridge_feature(f,axis!=0),material,0,false};return true;
    }
    return false;
}

int cave_environment_bridge_progress(int y,int x)
{
    return state.ready&&in_bounds_fully(y,x)?cells[y][x].work:0;
}

bool cave_environment_bridge_work(int y,int x,int material)
{
    environment_bridge_job job;
    if(!cave_environment_bridge_job_at(y,x,&job)||job.material!=material)return false;
    environment_cell* c=&cells[y][x];
    int required=job.integrity?4:material==ENV_BRIDGE_WOOD?8:16;
    if(++c->work<required)return false;
    /* Reinforcing an intact occupied deck changes no collision or underlay. */
    if (cave_feat[y][x] == job.feature) cave_event_emit(CAVE_EVENT_BRIDGE,y,x,12);
    else if(!commit(y,x,job.feature,CAVE_EVENT_BRIDGE)){c->work--;return false;}
    c->flags|=ENV_BRIDGE;c->bridge_feat=job.feature;c->material=material;
    if(!job.repair)c->underlay=cave_bridge_underlay(job.feature);
    c->integrity=100;c->work=0;
    cave_environment_observe(y,x);
    return true;
}

bool cave_environment_describe(int y,int x,char* text,size_t size)
{
    if(!text||!size)return false;
    text[0]='\0';
    if(!state.ready||!in_bounds_fully(y,x)||!(cave_info[y][x]&CAVE_SEEN))return false;
    environment_cell* c=&cells[y][x];
    if(c->pending_feat)strnfmt(text,size,"Unstable: %s",c->pending_feat==FEAT_CHASM?"the ground is splitting":
        c->pending_feat==FEAT_RUBBLE?"stone is crumbling":c->pending_feat==FEAT_LAVA?"lava is rising":"a flood is approaching");
    else if(c->flags&ENV_BRIDGE)strnfmt(text,size,"%s crossing: %s%s",c->material==ENV_BRIDGE_WOOD?"Wooden":"Stone",
        !c->integrity?"destroyed":c->integrity<=25?"failing":c->integrity<100?"damaged":"sound",c->work?" (repairs underway)":"");
    else if(c->flags&ENV_FLOOR_ICE)strnfmt(text,size,"Frozen coating over solid ground");
    else if(c->work)strnfmt(text,size,"A crossing is being built");
    return text[0]!='\0';
}

const environment_cell* cave_environment_cell_at(int y,int x){return in_bounds(y,x)?&cells[y][x]:NULL;}
const environment_source* cave_environment_source_at(int i){return i>=0&&i<state.source_count?&sources[i]:NULL;}
environment_state cave_environment_get_state(void){return state;}
bool cave_environment_restore_state(environment_state value)
{
    if(value.source_count>ENV_SOURCES_MAX||value.mineral_budget>8||(value.ready&&!value.random)||value.last_turn<0||value.last_turn>turn)return false;
    state=value;load_config();return true;
}
bool cave_environment_restore_cell(int y,int x,environment_cell c)
{
    if(!in_bounds(y,x)||c.flags>127||c.base_feat>=FEAT_COUNT||c.known_feat>=FEAT_COUNT
        ||c.bridge_feat>=FEAT_COUNT||c.underlay>=FEAT_COUNT||c.material>ENV_BRIDGE_STONE
        ||c.integrity>100||c.work>16||c.pending_feat>=FEAT_COUNT||c.owner>state.source_count
        ||c.known_underlay>=FEAT_COUNT||c.known_material>ENV_BRIDGE_STONE
        ||c.heat< -12||c.heat>12||c.due<0||(c.pending_feat&&c.due>turn+100000))return false;
    if(cells[y][x].pending_feat&&pending_count)pending_count--;
    cells[y][x]=c;if(c.pending_feat)pending_count++;return true;
}
bool cave_environment_restore_source(int i,environment_source s)
{
    if(i<0||i>=state.source_count||!in_bounds_fully(s.y,s.x)||!liquid(s.feature)
        ||!s.capacity||s.capacity>64||s.used>s.capacity||s.phase>12||s.next_turn<0||s.next_turn>turn+100000)return false;
    sources[i]=s;return true;
}

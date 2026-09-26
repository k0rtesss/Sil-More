#include "angband.h"
#include "externs.h"
#include "cave/cave-environment.h"
#include "cave/cave-events.h"
#include "cave/cave-flood.h"
#include "cave/cave-bridge.h"
#include "log/log.h"
#include "spell/spell-damage.h"
#include "spell/spell-projection-internal.h"
#include "score/score_guid.h"
#include "player/killer.h"
#include <limits.h>

static catastrophe_state wrath;
/* step + 1 at first contact; zero means this catastrophe has not reached it. */
static u32b reached[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte visited[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
/* Per-action scratch only. Saved cell ages carry the propagation clock. */
static byte contacted[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static int queue[MAX_DUNGEON_HGT * MAX_DUNGEON_WID];
static guid64* acquired;
static int acquired_count, acquired_capacity;
static bool advanced, processing, legacy_baseline;

static bool playable(void)
{
    return p_ptr && p_ptr->game_type == 0 && p_ptr->depth > 0
        && !p_ptr->is_dead;
}

static unsigned random_below(unsigned limit)
{
    if (!wrath.random) wrath.random = 0x9e3779b9U ^ (u32b)turn;
    if (!wrath.random) wrath.random = 1;
    wrath.random ^= wrath.random << 13;
    wrath.random ^= wrath.random >> 17;
    wrath.random ^= wrath.random << 5;
    return limit ? wrath.random % limit : 0;
}

static int power(void) { return MAX(0, MIN(6, p_ptr->morgoth_state)); }
static int depth_chance(void) { return MAX(1, MIN(100, p_ptr->depth)); }
static int feature_for(int kind)
{
    static const int features[CATA_KIND_MAX] = {
        0, FEAT_WATER, FEAT_POISON, FEAT_LAVA, FEAT_ICE, FEAT_CHASM
    };
    return kind > CATA_AUTO && kind < CATA_KIND_MAX ? features[kind] : 0;
}

cptr catastrophe_name(int kind)
{
    static cptr names[CATA_KIND_MAX] = {
        "automatic", "water flood", "poisonous acid flood", "lava flood",
        "spreading ice", "spreading chasms"
    };
    return kind >= 0 && kind < CATA_KIND_MAX ? names[kind] : "unknown";
}

bool catastrophe_active(void) { return wrath.kind != CATA_AUTO; }
bool catastrophe_owns(int y, int x)
{
    return catastrophe_active() && in_bounds(y, x) && reached[y][x] != 0;
}

int catastrophe_chance(void)
{
    if (!p_ptr || p_ptr->depth <= 0) return 0;
    if (catastrophe_active() || p_ptr->on_the_run) return 100;
    return MAX(wrath.chance, depth_chance());
}

void catastrophe_format(char* buf, size_t size)
{
    if (catastrophe_active()) strnfmt(buf, size, "100%% - active");
    else strnfmt(buf, size, "%d%%", catastrophe_chance());
}

void catastrophe_reset_level(void)
{
    wrath.kind = CATA_AUTO; wrath.y = wrath.x = 0;
    wrath.step = 0; wrath.ready = false;
    memset(reached, 0, sizeof(reached));
    advanced = processing = false;
}

void catastrophe_reset_run(void)
{
    memset(&wrath, 0, sizeof(wrath));
    wrath.craft_difficulty = -1;
    wrath.last_stage = -1;
    acquired_count = 0;
    legacy_baseline = false;
    catastrophe_reset_level();
}

bool catastrophe_protected(int y, int x)
{
    if (!in_bounds_fully(y, x)) return true;
    int f = cave_feat[y][x];
    if (f == FEAT_NONE || f == FEAT_WALL_PERM || f == FEAT_SUNLIGHT
        || cave_stair_bold(y, x) || cave_forge_bold(y, x)) return true;
    for (object_type* obj = get_first_object(y, x); obj; obj = get_next_object(obj))
        if ((obj->tval == TV_LIGHT && obj->sval == SV_LIGHT_SILMARIL)
            || (obj->name1 >= ART_MORGOTH_0 && obj->name1 <= ART_MORGOTH_3))
            return true;
    return false;
}

static bool material(int kind, int feat)
{
    feat = cave_bridge_underlay(feat);
    return kind == CATA_WATER ? feat == FEAT_WATER || feat == FEAT_DEEP_WATER
        : kind == CATA_ICE ? FEAT_IS_ICE(feat) : feat == feature_for(kind);
}

static int actual_feature(int y, int x)
{
    int f=cave_feat[y][x];
    const environment_cell* c=cave_environment_cell_at(y,x);
    if(c && (c->flags&ENV_BRIDGE) && c->integrity
        && (FEAT_IS_BRIDGE(f)||f==c->bridge_feat))return c->underlay;
    return f;
}

static bool ordinary_ground(int f)
{
    return f == FEAT_FLOOR || f == FEAT_OPEN || f == FEAT_BROKEN
        || f == FEAT_RUBBLE || FEAT_IS_TRAP(f) || FEAT_IS_BRIDGE(f)
        || (f >= FEAT_DOOR_HEAD && f <= FEAT_DOOR_TAIL)
        || f == FEAT_SECRET || f == FEAT_WARDED || f == FEAT_WARDED2
        || f == FEAT_WARDED3;
}

static bool source_at(int kind, int y, int x)
{
    if (catastrophe_protected(y, x)) return false;
    int f = cave_feat[y][x];
    if (material(kind, actual_feature(y,x))) return true;
    if (f != FEAT_FLOOR) return false;
    int cave = level_partition_big_cave_type_for_point(y, x);
    if ((kind == CATA_LAVA && cave == BIG_CAVE_FIRE)
        || (kind == CATA_ACID && cave == BIG_CAVE_POIS)
        || (kind == CATA_ICE && cave == BIG_CAVE_ICE)) return true;
    if (kind != CATA_WATER
        || level_partition_kind_for_point(y, x) != LEVEL_PART_CAVEY) return false;
    return FEAT_IS_ROCK(cave_feat[y-1][x]) || FEAT_IS_ROCK(cave_feat[y+1][x])
        || FEAT_IS_ROCK(cave_feat[y][x-1]) || FEAT_IS_ROCK(cave_feat[y][x+1]);
}

bool catastrophe_can_start(int kind)
{
    if (!playable() || kind < CATA_AUTO || kind >= CATA_KIND_MAX) return false;
    for (int y = 1; y < p_ptr->cur_map_hgt-1; y++)
        for (int x = 1; x < p_ptr->cur_map_wid-1; x++)
            for (int k = kind ? kind : 1; k <= (kind ? kind : CATA_KIND_MAX-1); k++)
                if (source_at(k, y, x)) return true;
    return false;
}

void catastrophe_prepare_level(void)
{
    catastrophe_reset_level();
    wrath.ready = true;
    if (!playable() || !p_ptr->on_the_run || catastrophe_can_start(CATA_AUTO)) return;
    /* A connected natural spring in ordinary rock, never a partition-label
     * earthquake. Do this before the environment is seeded. */
    int sy = -1, sx = -1, count = 0;
    for (int y = 2; y < p_ptr->cur_map_hgt-2; y++)
        for (int x = 2; x < p_ptr->cur_map_wid-2; x++) {
            if (catastrophe_protected(y, x) || !FEAT_IS_ROCK(cave_feat[y][x])
                || (cave_info[y][x] & CAVE_G_VAULT)) continue;
            if (cave_feat[y-1][x] != FEAT_FLOOR && cave_feat[y+1][x] != FEAT_FLOOR
                && cave_feat[y][x-1] != FEAT_FLOOR && cave_feat[y][x+1] != FEAT_FLOOR) continue;
            if (!random_below(++count)) { sy = y; sx = x; }
        }
    if (sy >= 0) {
        cave_natural[sy][sx] = true;
        cave_set_feat(sy, sx, FEAT_WATER);
    } else {
        /* Entirely authored levels can have no diggable boundary. A small
         * spring on unoccupied floor still supplies a material source. */
        for (int y = 1; y < p_ptr->cur_map_hgt-1 && sy < 0; y++)
            for (int x = 1; x < p_ptr->cur_map_wid-1; x++)
                if (cave_feat[y][x] == FEAT_FLOOR && !cave_m_idx[y][x]
                    && !catastrophe_protected(y, x)) {
                    cave_natural[y][x] = true;
                    cave_set_feat(y, x, FEAT_WATER); sy = y; break;
                }
    }
}

static bool traverse(int y, int x)
{
    if (!in_bounds_fully(y, x)) return false;
    int f = cave_feat[y][x];
    /* Cold crosses solid structures without replacing or opening them.
     * Only the map boundary is a boundary of this field. */
    if (wrath.kind == CATA_ICE) return f != FEAT_NONE;
    if (f == FEAT_WALL_PERM || f == FEAT_NONE || FEAT_IS_ROCK(f)) return false;
    if (catastrophe_protected(y, x))
        return f != FEAT_SUNLIGHT; /* Keep support but do not make an invisible dam. */
    if (material(wrath.kind, actual_feature(y,x)) || ordinary_ground(f)) return true;
    if (wrath.kind == CATA_CHASM) return f == FEAT_WATER || FEAT_IS_ICE(f);
    return false;
}

static bool catastrophe_corner_ground(int y, int x, int unused)
{
    (void)unused;
    return traverse(y, x);
}

static bool convert(int y, int x, int feature)
{
    if (catastrophe_protected(y, x)) return false;
    bool changed = cave_environment_catastrophe_change(y, x, feature);
    if (!changed) return false;
    if (feature == FEAT_WATER || feature == FEAT_DEEP_WATER || feature == FEAT_POISON)
        cave_flood_restore_surface(y, x, feature == FEAT_POISON
            ? CAVE_FLOOD_KIND_ACID : CAVE_FLOOD_KIND_WATER);
    if (feature == FEAT_CHASM) {
        if (p_ptr->py == y && p_ptr->px == x && !p_ptr->leaping) hit_trap(y, x);
        else if (cave_m_idx[y][x] > 0) m_fall_in_chasm(y, x);
    }
    return true;
}

static void cold_pulse(void)
{
    int dd = power() + 2;
    if (catastrophe_owns(p_ptr->py, p_ptr->px)) {
        killer_mark_other(SCORE_KILLER_OTHER);
        cold_dam_pure(dd, 4, false, "Morgoth's freezing wrath");
    }
    for (int i = 1; i < mon_max && !p_ptr->is_dead && !p_ptr->leaving; i++) {
        monster_type* m = &mon_list[i];
        if (!m->r_idx || !catastrophe_owns(m->fy, m->fx)) continue;
        (void)project_m(0,m->fy,m->fx,dd,4,0,GF_COLD,PROJECT_KILL);
    }
}

/* One physical event per action, located on the changed front rather than
 * forever at its historical origin. No change means no new flood sound. */
static void note_activity(int y, int x, int* event_y, int* event_x)
{
    if (*event_y < 0 && material(wrath.kind,cave_feat[y][x])) {
        *event_y=y;
        *event_x=x;
    }
}

/* These are source budgets, not an ever-growing multiplier. Water spends
 * the same supply on wetting ground and raising existing water. Cold pays
 * for cooling solid terrain too; it never uses liquid collision rules. */
static int action_supply(int a)
{
    if (wrath.kind == CATA_ICE) return 128 + 32*a;
    if (wrath.kind == CATA_LAVA) return 24 + 6*a;
    if (wrath.kind == CATA_CHASM) return 16 + 4*a;
    return 32 + 8*a;
}

static unsigned transmission_delay(int y, int x, int a)
{
    int f = cave_feat[y][x];
    /* A saved arrival phase distributes one/two-action transmission across
     * the front. Intermediate anger therefore matters in narrow passages,
     * not only when a broad front exhausts its source budget. */
    u32b phase = reached[y][x] ^ (u32b)y*0x9e3779b9U ^ (u32b)x*0x85ebca6bU;
    phase ^= phase >> 16; phase *= 0x7feb352dU; phase ^= phase >> 15;
    unsigned delay = phase%6 < (unsigned)a ? 1 : 2;
    if (wrath.kind == CATA_ICE) {
        bool solid = FEAT_IS_ROCK(f) || f == FEAT_WALL_PERM
            || (f >= FEAT_DOOR_HEAD && f <= FEAT_DOOR_TAIL)
            || f == FEAT_SECRET || f == FEAT_WARDED || f == FEAT_WARDED2
            || f == FEAT_WARDED3;
        return delay + (solid ? 1 : 0);
    }
    if (wrath.kind == CATA_LAVA || wrath.kind == CATA_CHASM)
        return delay;
    return 1;
}

static bool connected_material(int y, int x)
{
    if (!traverse(y,x)) return false;
    return wrath.kind == CATA_ICE || catastrophe_protected(y,x)
        || material(wrath.kind,actual_feature(y,x));
}

static void freeze_surface(int y, int x, int a, int* event_y, int* event_x)
{
    int f = cave_feat[y][x];
    if (catastrophe_protected(y,x)) return;
    /* Coating a structure does not remove its door, trap, deck or wall.
     * The cold field (and exposure) exists independently of FEAT_ICE. */
    if (f != FEAT_FLOOR && f != FEAT_WATER && f != FEAT_DEEP_WATER) return;
    if ((f == FEAT_WATER || f == FEAT_DEEP_WATER)
        && wrath.step+1-reached[y][x] < (unsigned)(3-a/3)) return;
    /* Keep the shared melting interaction; do not refreeze a hot fringe
     * immediately after the reaction pass has just melted it. */
    for (int d=0;d<8;d++)
        if (actual_feature(y+ddy_ddd[d],x+ddx_ddd[d]) == FEAT_LAVA) return;
    if (convert(y,x,FEAT_ICE)) note_activity(y,x,event_y,event_x);
}

static void advance(void)
{
    static byte available[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    if (!catastrophe_active() || processing || !playable() || p_ptr->leaving || cheat_timestop) return;
    processing = advanced = true;
    if (wrath.step < UINT_MAX-1) wrath.step++;
    int a = power(), f = feature_for(wrath.kind);
    int quota = action_supply(a), supply = quota;
    int wear = wrath.kind == CATA_ACID ? 5+2*a : wrath.kind == CATA_LAVA ? 15+3*a
        : wrath.kind == CATA_CHASM ? 20+4*a : 0;
    int bridge_force = 8+4*a;
    bool liquid = wrath.kind == CATA_WATER || wrath.kind == CATA_ACID;
    int phase = (wrath.step-1)%6;
    int rounds = liquid ? 1 + (phase+1)*a/6 - phase*a/6 : 1;
    int changed=0, deepened=0, tail=0;
    int event_y=-1, event_x=-1, deepen_left=quota/4;
    unsigned deepen_delay = 6-a/2;
    cave_environment_catastrophe_react();
    memset(contacted,0,sizeof(contacted));
    const environment_cell* origin=cave_environment_cell_at(wrath.y,wrath.x);
    if (origin && (origin->flags&ENV_BRIDGE) && origin->integrity) {
        int old=cave_feat[wrath.y][wrath.x];
        contacted[wrath.y][wrath.x]=1;
        (void)cave_environment_catastrophe_contact(wrath.y,wrath.x,f,wear,bridge_force);
        if (cave_feat[wrath.y][wrath.x]!=old)
            note_activity(wrath.y,wrath.x,&event_y,&event_x);
        if(cave_feat[wrath.y][wrath.x]==FEAT_CHASM)convert(wrath.y,wrath.x,FEAT_CHASM);
    }
    for (int round=0;round<rounds && !p_ptr->is_dead && !p_ptr->leaving;round++) {
        /* A snapshot bounds travel to one new layer per substep. Newly
         * changed cells cannot recursively consume a corridor in this BFS. */
        for(int y=0;y<p_ptr->cur_map_hgt;y++)for(int x=0;x<p_ptr->cur_map_wid;x++)
            available[y][x]=reached[y][x] && connected_material(y,x);
        memset(visited,0,sizeof(visited));
        int head=0;tail=0;
        if (available[wrath.y][wrath.x]) {
            queue[tail++]=wrath.y*MAX_DUNGEON_WID+wrath.x;
            visited[wrath.y][wrath.x]=1;
        }
        while(head<tail && !p_ptr->is_dead && !p_ptr->leaving) {
            int pos=queue[head++],y=pos/MAX_DUNGEON_WID,x=pos%MAX_DUNGEON_WID;
            /* A surviving deck can conduct its underlying flow, but must
             * continue receiving contact on later actions as well. */
            const environment_cell* deck=cave_environment_cell_at(y,x);
            if (!contacted[y][x] && deck && (deck->flags&ENV_BRIDGE) && deck->integrity
                && (FEAT_IS_BRIDGE(cave_feat[y][x]) || cave_feat[y][x]==deck->bridge_feat)) {
                int old=cave_feat[y][x];
                contacted[y][x]=1;
                (void)cave_environment_catastrophe_contact(y,x,f,wear,bridge_force);
                if(cave_feat[y][x]!=old)note_activity(y,x,&event_y,&event_x);
                if(cave_feat[y][x]==FEAT_CHASM)convert(y,x,FEAT_CHASM);
                if(p_ptr->is_dead || p_ptr->leaving)break;
            }
            unsigned age=wrath.step+1-reached[y][x];
            bool emit=(y==wrath.y && x==wrath.x)
                || age>=transmission_delay(y,x,a) || (liquid && round>0);
            if (wrath.kind==CATA_ICE) freeze_surface(y,x,a,&event_y,&event_x);
            if (wrath.kind==CATA_WATER && deepen_left && supply
                && age>=deepen_delay && cave_feat[y][x]==FEAT_WATER
                && convert(y,x,FEAT_DEEP_WATER)) {
                deepen_left--;supply--;deepened++;note_activity(y,x,&event_y,&event_x);
            }
            int offset=random_below(8);
            for(int n=0;n<8;n++) {
                int d=(n+offset)%8,ny=y+ddy_ddd[d],nx=x+ddx_ddd[d];
                if (!in_bounds_fully(ny,nx) || visited[ny][nx]) continue;
                /* Cold is not liquid: it crosses even a sealed stone corner. */
                if (wrath.kind!=CATA_ICE
                    && !cave_flood_step_allowed(y,x,ny,nx,catastrophe_corner_ground,0)) continue;
                if (available[ny][nx]) {
                    visited[ny][nx]=1;queue[tail++]=ny*MAX_DUNGEON_WID+nx;continue;
                }
                if (!emit) continue;
                visited[ny][nx]=1;
                if (wrath.kind==CATA_ICE) {
                    if (!supply || !traverse(ny,nx)) continue;
                    reached[ny][nx]=wrath.step+1;supply--;changed++;
                    freeze_surface(ny,nx,a,&event_y,&event_x);
                    continue;
                }
                int old=cave_feat[ny][nx];
                bool protected=catastrophe_protected(ny,nx);
                const environment_cell* cell=cave_environment_cell_at(ny,nx);
                bool bridge=cell && (cell->flags&ENV_BRIDGE) && cell->integrity
                    && (FEAT_IS_BRIDGE(old) || old==cell->bridge_feat);
                if (!protected && (FEAT_IS_ROCK(old) || bridge)) {
                    if (!contacted[ny][nx]) {
                        contacted[ny][nx]=1;
                        if (cave_environment_catastrophe_contact(ny,nx,f,wear,bridge_force)
                            && !reached[ny][nx]) reached[ny][nx]=wrath.step+1;
                    }
                    if (cave_feat[ny][nx]!=old)note_activity(ny,nx,&event_y,&event_x);
                    old=cave_feat[ny][nx];
                    if (FEAT_IS_ROCK(old)) continue;
                    if (bridge && cave_environment_cell_at(ny,nx)->integrity) {
                        if (material(wrath.kind,actual_feature(ny,nx)) && !reached[ny][nx])
                            reached[ny][nx]=wrath.step+1;
                        continue;
                    }
                    if (bridge && old==FEAT_CHASM)convert(ny,nx,old);
                }
                if (!traverse(ny,nx)) continue;
                if (!protected && !material(wrath.kind,old)) {
                    if (!supply || !convert(ny,nx,f)) continue;
                    supply--;changed++;note_activity(ny,nx,&event_y,&event_x);
                }
                /* A breach is a new route even when prior wall contact had
                 * already marked the cell as owned. */
                reached[ny][nx]=wrath.step+1;
                if(p_ptr->is_dead || p_ptr->leaving)break;
            }
        }
    }
    if (wrath.kind==CATA_WATER && !p_ptr->is_dead && !p_ptr->leaving) {
        /* A narrow or blocked front can spend its remaining inflow filling
         * the basin. An expanding basin must share that inflow with spread. */
        for(int i=0;i<tail && supply;i++) {
            int pos=queue[i],y=pos/MAX_DUNGEON_WID,x=pos%MAX_DUNGEON_WID;
            if (wrath.step+1-reached[y][x]>=deepen_delay && cave_feat[y][x]==FEAT_WATER
                && convert(y,x,FEAT_DEEP_WATER)) {
                supply--;deepened++;note_activity(y,x,&event_y,&event_x);
            }
        }
    }
    if (wrath.kind==CATA_ICE && !p_ptr->is_dead && !p_ptr->leaving) cold_pulse();
    int event=wrath.kind==CATA_CHASM?CAVE_EVENT_COLLAPSE:wrath.kind==CATA_ICE?CAVE_EVENT_FREEZE
        :wrath.kind==CATA_LAVA?CAVE_EVENT_VENT:CAVE_EVENT_FLOOD;
    if(event_y>=0)cave_event_emit(event,event_y,event_x,24);
    p_ptr->update|=PU_UPDATE_VIEW|PU_MONSTERS;p_ptr->redraw|=PR_MAP;
    log_debug("WRATH step=%u kind=%d power=%d changed=%d supply=%d unused=%d deepened=%d reachable=%d origin=(%d,%d) source_feat=%d activity=(%d,%d)",
        wrath.step,wrath.kind,a,changed,quota,supply,deepened,tail,wrath.y,wrath.x,
        actual_feature(wrath.y,wrath.x),event_y,event_x);
    processing=false;
}

bool catastrophe_start(int kind)
{
    if (!playable() || !character_dungeon || p_ptr->leaving || catastrophe_active()) return false;
    if (kind == CATA_AUTO) {
        int choices[CATA_KIND_MAX], count = 0;
        for (int k = 1; k < CATA_KIND_MAX; k++)
            if (catastrophe_can_start(k)) choices[count++] = k;
        if (!count) return false;
        kind = choices[random_below(count)];
    }
    if (kind <= CATA_AUTO || kind >= CATA_KIND_MAX) return false;
    int sy = -1, sx = -1, count = 0;
    for (int y = 1; y < p_ptr->cur_map_hgt-1; y++)
        for (int x = 1; x < p_ptr->cur_map_wid-1; x++)
            if (source_at(kind,y,x) && !random_below(++count)) { sy=y; sx=x; }
    if (sy < 0) return false;
    if (!cave_environment_get_state().ready) cave_environment_seed();
    wrath.kind=kind; wrath.y=sy; wrath.x=sx; wrath.step=0; wrath.ready=true;
    wrath.chance=depth_chance();
    memset(reached,0,sizeof(reached)); reached[sy][sx]=1;
    msg_format("Morgoth's wrath unleashes %s!",catastrophe_name(kind));
    disturb(0,0);
    const environment_cell* source_cell=cave_environment_cell_at(sy,sx);
    if (!(source_cell && (source_cell->flags&ENV_BRIDGE) && source_cell->integrity))
        convert(sy,sx,feature_for(kind));
    log_info("WRATH started: kind=%d origin=(%d,%d) power=%d depth=%d",kind,sy,sx,power(),p_ptr->depth);
    advance();
    return true;
}

void catastrophe_note(enum catastrophe_event event)
{
    if (!playable() || processing || event < 0 || event >= CATA_EVENT_MAX) return;
    if (wrath.pending[event] < 4096) wrath.pending[event]++;
}

void catastrophe_flush_events(void)
{
    static const int increments[CATA_EVENT_MAX] = {5,7,5,3,3,1,1,2};
    static cptr names[CATA_EVENT_MAX] = {"unique enemy", "minimum depth", "Vala reward",
        "forged artefact", "acquired artefact", "thrall", "song of power", "attribute ability"};
    if (!playable() || !character_dungeon || p_ptr->leaving || processing) return;
    for (int e=0;e<CATA_EVENT_MAX;e++) while (wrath.pending[e]) {
        wrath.pending[e]--;
        if (catastrophe_active() || p_ptr->on_the_run) continue;
        int chance=catastrophe_chance();
        bool success=random_below(100)<(unsigned)chance;
        if (success && catastrophe_start(CATA_AUTO)) {
            if (e==CATA_TIMER) msg_format("Minimum-depth catastrophe roll: %d%%. Catastrophe!",chance);
        } else {
            wrath.chance=MIN(100,chance+increments[e]);
            if(e==CATA_TIMER)msg_format("Minimum-depth catastrophe roll: %d%%. No catastrophe; the next chance is %d%%.",chance,wrath.chance);
        }
        log_info("WRATH roll: event=%s chance=%d active=%d next=%d",names[e],chance,catastrophe_active(),wrath.chance);
        if(p_ptr->is_dead||p_ptr->leaving)return;
    }
}

void catastrophe_sync_depth(int stage) { wrath.last_stage=MAX(0,stage); }
void catastrophe_depth_tick(int stage)
{
    int threshold=0;
    min_depth_timer_status(NULL,NULL,NULL,NULL,&threshold);
    if(wrath.timer_step && wrath.timer_step!=threshold) wrath.last_stage=stage;
    wrath.timer_step=threshold;
    if (wrath.last_stage<0) wrath.last_stage=MAX(0,stage-1);
    if (stage<wrath.last_stage) { wrath.last_stage=stage; return; }
    while (wrath.last_stage<stage) {
        wrath.last_stage++;
        if (playable() && !p_ptr->on_the_run && wrath.last_stage+1>p_ptr->depth)
            catastrophe_note(CATA_TIMER);
    }
}

static void milestones(int difficulty, bool forged)
{
    byte* mask=forged?&wrath.smith_milestones:&wrath.find_milestones;
    for(int i=0;i<5;i++) if(difficulty>=15+10*i && !(*mask&(1<<i))) {
        *mask|=1<<i;
        catastrophe_note(forged?CATA_SMITH:CATA_FIND);
    }
}

static guid64 identity(const object_type* obj)
{
    int id=obj->name1;
    if(id>=ART_MORGOTH_0&&id<=ART_MORGOTH_3)id=ART_MORGOTH_0;
    guid64 guid=a_info[id].guid;
    if(score_guid_is_zero(&guid))guid=score_guid_from_u64((u64b)id+1);
    return guid;
}

bool catastrophe_restore_guid(guid64 guid)
{
    if(score_guid_is_zero(&guid))return false;
    for(int i=0;i<acquired_count;i++)if(!memcmp(&acquired[i],&guid,sizeof(guid)))return false;
    if(acquired_count>=acquired_capacity) {
        int cap=MAX(32,acquired_capacity*2);
        guid64* next=mem_alloc_array(cap,guid64);
        if(acquired_count)memcpy(next,acquired,acquired_count*sizeof(*next));
        mem_free(acquired); acquired=next; acquired_capacity=cap;
    }
    acquired[acquired_count++]=guid;
    return true;
}

void catastrophe_acquired(const object_type* obj)
{
    if(!obj||!obj->k_idx||obj->name1<=0||!a_info||!z_info||obj->name1>=z_info->art_max)return;
    if(!catastrophe_restore_guid(identity(obj)))return;
    if(!playable()||obj->name1>=z_info->art_rand_max)return; /* player-forged */
    milestones(object_intrinsic_difficulty(obj),false);
}

void catastrophe_scan_inventory(bool baseline)
{
    if(!p_ptr||!a_info||!z_info)return;
    int extra=player_carried_extra_entry_count();
    int quiver=player_quiver_store_entry_count();
    for(int i=0;i<INVEN_TOTAL+extra+quiver;i++) {
        const object_type* obj=i<INVEN_TOTAL?&inventory[i]
            :i<INVEN_TOTAL+extra?player_carried_extra_entry_at(i-INVEN_TOTAL)
            :player_quiver_store_entry_at(i-INVEN_TOTAL-extra);
        if(!obj||!obj->k_idx||!obj->name1||obj->name1>=z_info->art_max)continue;
        if(baseline)(void)catastrophe_restore_guid(identity(obj));
        else catastrophe_acquired(obj);
    }
}

void catastrophe_accept_craft(int difficulty) { wrath.craft_difficulty=difficulty; }
void catastrophe_crafted(const object_type* obj)
{
    if(obj&&obj->k_idx&&obj->name1) {
        (void)catastrophe_restore_guid(identity(obj));
        milestones(MAX(0,wrath.craft_difficulty),true);
    }
    wrath.craft_difficulty=-1;
}

void catastrophe_ability(int skill,int ability)
{
    if((skill==S_MEL&&ability==MEL_STR)||(skill==S_ARC&&ability==ARC_DEX)
        ||(skill==S_EVN&&ability==EVN_DEX)||(skill==S_STL&&ability==STL_DEX)
        ||(skill==S_PER&&ability==PER_GRA)||(skill==S_WIL&&ability==WIL_CON)
        ||(skill==S_SMT&&ability==SMT_GRA)||(skill==S_SNG&&ability==SNG_GRA))
        catastrophe_note(CATA_ABILITY);
}

void catastrophe_legacy_loaded(void) { legacy_baseline=true; }
void catastrophe_resume_level(void)
{
    if(!playable()||!character_dungeon||p_ptr->leaving)return;
    if(legacy_baseline) {
        catastrophe_scan_inventory(true); legacy_baseline=false;
        wrath.last_stage=min_depth_timer_stage();
    }
    if(wrath.last_stage<0) wrath.last_stage=min_depth_timer_stage();
    int threshold=0; min_depth_timer_status(NULL,NULL,NULL,NULL,&threshold);
    if(wrath.timer_step!=threshold) wrath.last_stage=min_depth_timer_stage();
    wrath.timer_step=threshold;
    wrath.chance=MAX(wrath.chance,depth_chance());
    if(p_ptr->on_the_run&&!catastrophe_active()) {
        if(!catastrophe_can_start(CATA_AUTO)) {
            catastrophe_prepare_level(); cave_environment_seed();
        }
        (void)catastrophe_start(CATA_AUTO);
    }
    wrath.ready=true;
}
void catastrophe_begin_action(void) { advanced=false; }
void catastrophe_end_action(void)
{
    if(p_ptr->on_the_run&&!catastrophe_active())catastrophe_resume_level();
    catastrophe_scan_inventory(false);
    catastrophe_flush_events();
    if(p_ptr->energy_use&&!advanced)advance();
}

catastrophe_state catastrophe_get_state(void) { return wrath; }
bool catastrophe_restore_state(catastrophe_state s)
{
    if(s.chance>100||s.smith_milestones>31||s.find_milestones>31||s.kind>=CATA_KIND_MAX
        ||s.last_stage< -1||s.craft_difficulty< -1||s.timer_step<0||s.step>=UINT_MAX-1
        ||(s.kind&&(!s.ready||!in_bounds_fully(s.y,s.x))))return false;
    for(int i=0;i<CATA_EVENT_MAX;i++)if(s.pending[i]>4096)return false;
    wrath=s; advanced=processing=false;
    return true;
}
u32b catastrophe_cell_step(int y,int x) { return reached[y][x]; }
bool catastrophe_restore_cell(int y,int x,u32b step)
{
    if(!in_bounds(y,x)||step>wrath.step+1||(!wrath.kind&&step))return false;
    reached[y][x]=step; return true;
}
int catastrophe_acquired_count(void) { return acquired_count; }
guid64 catastrophe_acquired_guid(int i) { return acquired[i]; }

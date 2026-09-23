#include "angband.h"
#include "externs.h"
#include "cave/cave-bridge.h"
#include "cave/cave-events.h"
#include "cave/cave-water-flow.h"

#define SOUND_CELLS (MAX_DUNGEON_HGT * MAX_DUNGEON_WID)
#define SOUND_LIFETIME 60
static cave_world_event events[CAVE_EVENTS_MAX];
static u32b next_serial = 1, revision = 1;
static byte fields[CAVE_EVENTS_MAX][MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static u32b field_revision[CAVE_EVENTS_MAX], field_serial[CAVE_EVENTS_MAX];
static byte ambient[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte flowing_liquid_ambient[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte still_liquid_ambient[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte lava_ambient[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte forge_ambient[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte bridge_work_sources[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte bridge_work_ambient[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static s32b ambient_turn = -1, last_notice[CAVE_EVENT_MAX];
static s32b bridge_work_turn = -1, bridge_work_field_turn = -1;
static u32b bridge_work_revision = 1;
static u32b bridge_work_field_revision, bridge_work_field_terrain_revision;
static s32b ambient_notice_turn = -200;
static bool in_ambient_noise;
static u32b ambient_revision, announced[CAVE_EVENTS_MAX];
static int queue[SOUND_CELLS], head, tail, count;
static bool queued[MAX_DUNGEON_HGT][MAX_DUNGEON_WID], player_moved;
static const char* event_noise(int kind);

/* A forge's feature value is also its remaining-use counter.  The first
 * feature in each quality range is depleted and must not emit forge fire. */
static bool cave_forge_has_fire(int feat)
{
    return ((feat > FEAT_FORGE_NORMAL_HEAD && feat <= FEAT_FORGE_NORMAL_TAIL)
        || (feat > FEAT_FORGE_GOOD_HEAD && feat <= FEAT_FORGE_GOOD_TAIL)
        || (feat > FEAT_FORGE_UNIQUE_HEAD && feat <= FEAT_FORGE_UNIQUE_TAIL));
}

void cave_events_player_moved(bool moved) { player_moved = moved; }
bool cave_events_player_is_moving(void) { return player_moved; }
static bool sound_open(int y, int x)
{
    return in_bounds(y, x) && (!cave_wall_bold(y, x)
        || cave_any_closed_door_bold(y, x));
}
static void enqueue(int y, int x)
{
    if (queued[y][x]) return;
    queued[y][x] = true;
    queue[tail] = y * MAX_DUNGEON_WID + x;
    tail = (tail + 1) % SOUND_CELLS;
    count++;
}
static void begin_field(void)
{
    head = tail = count = 0;
    memset(queued, 0, sizeof(queued));
}
/* Positive costs and monotonically increasing, capped intensities bound work.
 * A cell is queued only once at a time, even for a field with many sources. */
static void propagate(byte field[MAX_DUNGEON_HGT][MAX_DUNGEON_WID])
{
    while (count)
    {
        int pos = queue[head], y = pos / MAX_DUNGEON_WID;
        int x = pos % MAX_DUNGEON_WID, dy, dx;
        head = (head + 1) % SOUND_CELLS; count--; queued[y][x] = false;
        for (dy = -1; dy <= 1; dy++) for (dx = -1; dx <= 1; dx++)
        {
            int ny = y + dy, nx = x + dx, strength;
            if ((!dy && !dx) || !sound_open(ny, nx)) continue;
            /* Neither walls nor closed doors permit diagonal corner leaks. */
            if (dy && dx && (!sound_open(y, nx) || !sound_open(ny, x)
                || cave_any_closed_door_bold(y, nx)
                || cave_any_closed_door_bold(ny, x))) continue;
            strength = field[y][x] - (cave_any_closed_door_bold(ny, nx) ? 5 : 1);
            if (strength <= field[ny][nx]) continue;
            field[ny][nx] = strength; enqueue(ny, nx);
        }
    }
}
static bool recorded(const cave_world_event* e)
{
    return e->serial && e->kind > CAVE_EVENT_NONE && e->kind < CAVE_EVENT_MAX
        && in_bounds(e->y, e->x) && e->turn >= 0 && e->turn <= turn;
}
static bool active(const cave_world_event* e)
{
    return recorded(e) && turn - e->turn < SOUND_LIFETIME;
}
static int event_strength(int i, int y, int x)
{
    cave_world_event* e = &events[i];
    if (!active(e) || !in_bounds(y, x)) return 0;
    if (field_revision[i] != revision || field_serial[i] != e->serial)
    {
        memset(fields[i], 0, sizeof(fields[i])); begin_field();
        fields[i][e->y][e->x] = MIN(64, e->volume);
        enqueue(e->y, e->x); propagate(fields[i]);
        field_revision[i] = revision; field_serial[i] = e->serial;
    }
    return MAX(0, fields[i][y][x] - ((turn - e->turn) / 10) * 2);
}
void cave_events_terrain_changed(void) { revision++; }
void cave_events_reset(void)
{
    memset(events, 0, sizeof(events)); memset(field_serial, 0, sizeof(field_serial));
    memset(announced, 0, sizeof(announced));
    memset(bridge_work_sources, 0, sizeof(bridge_work_sources));
    memset(bridge_work_ambient, 0, sizeof(bridge_work_ambient));
    memset(last_notice, -1, sizeof(last_notice));
    next_serial = 1; revision++; ambient_turn = -1; player_moved = false;
    bridge_work_turn = -1; bridge_work_field_turn = -1;
    bridge_work_revision++;
    bridge_work_field_revision = 0;
    bridge_work_field_terrain_revision = 0;
    ambient_revision = 0;
    in_ambient_noise = false; ambient_notice_turn = turn - 200;
}
void cave_event_emit(int kind, int y, int x, int volume)
{
    cave_world_event* e;
    if (kind <= CAVE_EVENT_NONE || kind >= CAVE_EVENT_MAX || !in_bounds(y, x)) return;
    e = &events[(next_serial - 1) % CAVE_EVENTS_MAX];
    e->serial = next_serial++; e->turn = turn;
    e->kind = kind; e->y = y; e->x = x; e->volume = MAX(1, MIN(64, volume));
    /* Log at emission: hearing filters, cooldowns and ring-buffer replacement
     * must not hide debug events. Normal hearing and AI remain unchanged. */
    if (op_ptr->opt[OPT_show_dungeon_events])
        msg_format("[Dungeon event %lu] %s at (y=%d, x=%d), turn %ld, volume %u.",
            (unsigned long)e->serial, event_noise(e->kind), y, x,
            (long)e->turn, (unsigned int)e->volume);
}
bool cave_event_latest(cave_world_event* event)
{
    const cave_world_event* latest = NULL;
    for (int i = 0; i < CAVE_EVENTS_MAX; i++)
        if (recorded(&events[i]) && (!latest || events[i].serial > latest->serial))
            latest = &events[i];
    if (!latest) return false;
    if (event) *event = *latest;
    return true;
}
bool cave_event_for_listener(int y, int x, int perception, u32b after,
    cave_world_event* event)
{
    int i, best = -1, score = 0;
    for (i = 0; i < CAVE_EVENTS_MAX; i++)
    {
        int strength;
        if (events[i].serial <= after) continue;
        strength = event_strength(i, y, x);
        if (strength > 0 && strength + perception / 3 > score)
        { best = i; score = strength + perception / 3; }
    }
    if (best < 0) return false;
    if (event) *event = events[best];
    return true;
}
static void refresh_ambient_fields(void)
{
    int yy, xx;

    if (!p_ptr || (ambient_turn == turn / 10
        && ambient_revision == revision))
        return;

    memset(ambient, 0, sizeof(ambient));
    memset(flowing_liquid_ambient, 0, sizeof(flowing_liquid_ambient));
    memset(still_liquid_ambient, 0, sizeof(still_liquid_ambient));
    memset(lava_ambient, 0, sizeof(lava_ambient));
    memset(forge_ambient, 0, sizeof(forge_ambient));
    begin_field();
    for (yy = 0; yy < p_ptr->cur_map_hgt; yy++)
        for (xx = 0; xx < p_ptr->cur_map_wid; xx++)
        {
            int feat = cave_feat[yy][xx];
            int underlay = cave_bridge_underlay(feat);
            bool moving = cave_water_flow_direction(yy, xx)
                != CAVE_WATER_FLOW_CALM;
            bool flow_metadata_missing = !cave_water_flow_is_valid();

            /* Separate running channels from still pools and lakes. Older
             * saves lack the flow graph, so keep their liquid audible as
             * flowing water through the compatibility fallback. */
            if (underlay == FEAT_WATER || underlay == FEAT_DEEP_WATER
                || underlay == FEAT_POISON)
            {
                if (moving || flow_metadata_missing)
                    flowing_liquid_ambient[yy][xx]
                        = CAVE_FLOWING_LIQUID_SOUND_RADIUS + 1;
                else
                    still_liquid_ambient[yy][xx]
                        = CAVE_FLOWING_LIQUID_SOUND_RADIUS + 1;
            }

            if (underlay == FEAT_LAVA)
                lava_ambient[yy][xx] = CAVE_LAVA_SOUND_RADIUS + 1;

            if (cave_forge_has_fire(feat))
                forge_ambient[yy][xx] = CAVE_FORGE_SOUND_RADIUS + 1;

            /* Still pools do not add noise to the discrete dungeon-event mask. */
            if ((feat == FEAT_WATER || feat == FEAT_DEEP_WATER
                || feat == FEAT_POISON || FEAT_IS_BRIDGE(feat)) && moving)
                ambient[yy][xx] = 8;
            else if (feat == FEAT_LAVA)
                ambient[yy][xx] = 5;
            if (ambient[yy][xx]) enqueue(yy, xx);
        }
    propagate(ambient);

    /* Reuse the same wall/door-aware propagation for the river field. This
     * deliberately reaches open cells beside the liquid, not just liquid
     * tiles themselves. */
    begin_field();
    for (yy = 0; yy < p_ptr->cur_map_hgt; yy++)
        for (xx = 0; xx < p_ptr->cur_map_wid; xx++)
            if (flowing_liquid_ambient[yy][xx])
                enqueue(yy, xx);
    propagate(flowing_liquid_ambient);

    /* Still water and acid pools have an independent field and loop, allowing
     * them to mix with nearby running water at their respective distances. */
    begin_field();
    for (yy = 0; yy < p_ptr->cur_map_hgt; yy++)
        for (xx = 0; xx < p_ptr->cur_map_wid; xx++)
            if (still_liquid_ambient[yy][xx])
                enqueue(yy, xx);
    propagate(still_liquid_ambient);

    /* Lava has its own loop, but uses the same wall/door-aware propagation. */
    begin_field();
    for (yy = 0; yy < p_ptr->cur_map_hgt; yy++)
        for (xx = 0; xx < p_ptr->cur_map_wid; xx++)
            if (lava_ambient[yy][xx])
                enqueue(yy, xx);
    propagate(lava_ambient);

    /* Forge fire uses the same wall/door-aware propagation as the other
     * environmental loops, so nearby open cells also hear it. */
    begin_field();
    for (yy = 0; yy < p_ptr->cur_map_hgt; yy++)
        for (xx = 0; xx < p_ptr->cur_map_wid; xx++)
            if (forge_ambient[yy][xx])
                enqueue(yy, xx);
    propagate(forge_ambient);

    ambient_turn = turn / 10;
    ambient_revision = revision;
}

int cave_flowing_water_sound_level_at(int y, int x)
{
    if (!in_bounds(y, x)) return 0;
    refresh_ambient_fields();
    return flowing_liquid_ambient[y][x];
}

void cave_events_note_bridge_work(int y, int x)
{
    if (!in_bounds(y, x)) return;
    if (bridge_work_turn != turn)
    {
        memset(bridge_work_sources, 0, sizeof(bridge_work_sources));
        bridge_work_turn = turn;
    }
    bridge_work_sources[y][x] = CAVE_FLOWING_LIQUID_SOUND_RADIUS + 1;
    bridge_work_revision++;
}

static void refresh_bridge_work_field(void)
{
    if (!p_ptr || (bridge_work_field_turn == turn
        && bridge_work_field_revision == bridge_work_revision
        && bridge_work_field_terrain_revision == revision))
        return;

    memset(bridge_work_ambient, 0, sizeof(bridge_work_ambient));
    begin_field();
    if (bridge_work_turn == turn)
    {
        for (int yy = 0; yy < p_ptr->cur_map_hgt; yy++)
            for (int xx = 0; xx < p_ptr->cur_map_wid; xx++)
                if (bridge_work_sources[yy][xx] && sound_open(yy, xx))
                {
                    bridge_work_ambient[yy][xx] = bridge_work_sources[yy][xx];
                    enqueue(yy, xx);
                }
    }
    propagate(bridge_work_ambient);
    bridge_work_field_turn = turn;
    bridge_work_field_revision = bridge_work_revision;
    bridge_work_field_terrain_revision = revision;
}

int cave_bridge_work_sound_level_at(int y, int x)
{
    if (!in_bounds(y, x)) return 0;
    refresh_bridge_work_field();
    return bridge_work_ambient[y][x];
}

int cave_still_liquid_sound_level_at(int y, int x)
{
    if (!in_bounds(y, x)) return 0;
    refresh_ambient_fields();
    return still_liquid_ambient[y][x];
}

int cave_lava_sound_level_at(int y, int x)
{
    if (!in_bounds(y, x)) return 0;
    refresh_ambient_fields();
    return lava_ambient[y][x];
}

int cave_forge_sound_level_at(int y, int x)
{
    if (!in_bounds(y, x)) return 0;
    refresh_ambient_fields();
    return forge_ambient[y][x];
}

int cave_sound_mask_at(int y, int x)
{
    int i, mask;
    if (!in_bounds(y, x)) return 0;
    refresh_ambient_fields();
    mask = ambient[y][x];
    for (i = 0; i < CAVE_EVENTS_MAX; i++)
        mask = MAX(mask, event_strength(i, y, x) / 3);
    return MIN(10, mask);
}
int cave_fighting_mask_at(int y, int x)
{
    int mask = 0;
    for (int i = 0; i < CAVE_EVENTS_MAX; i++)
        if (events[i].kind == CAVE_EVENT_FIGHT)
            mask = MAX(mask, event_strength(i, y, x) / 3);
    return MIN(10, mask);
}

static const char* event_noise(int kind)
{
    switch (kind)
    {
        case CAVE_EVENT_CRACK: return "stone cracking";
        case CAVE_EVENT_COLLAPSE: return "a thunderous collapse";
        case CAVE_EVENT_FLOOD: return "a rushing flood";
        case CAVE_EVENT_BUILD: return "rhythmic hammering";
        case CAVE_EVENT_DIG: return "scraping and falling stones";
        case CAVE_EVENT_FREEZE: return "ice creaking";
        case CAVE_EVENT_THAW: return "ice breaking and water dripping";
        case CAVE_EVENT_BRIDGE: return "timbers and masonry shifting";
        case CAVE_EVENT_VENT: return "a hissing vent";
        case CAVE_EVENT_FIGHT: return "shouts and clashing weapons";
        default: return "an ominous rumble";
    }
}
void cave_events_process(void)
{
    int i, notices = 0;
    for (i = 0; i < CAVE_EVENTS_MAX; i++)
    {
        cave_world_event* e = &events[i];
        int dy, dx, strength;
        bool danger;
        const char* direction;
        if (announced[i] == e->serial || !active(e)) continue;
        strength = event_strength(i, p_ptr->py, p_ptr->px);
        if (!strength) continue;
        danger = (e->kind == CAVE_EVENT_WARNING || e->kind == CAVE_EVENT_COLLAPSE
            || e->kind == CAVE_EVENT_CRACK || e->kind == CAVE_EVENT_FLOOD || e->kind == CAVE_EVENT_VENT)
            && ABS((int)e->y - p_ptr->py) <= 3 && ABS((int)e->x - p_ptr->px) <= 3;
        announced[i] = e->serial;
        if (danger) disturb(0, 0);
        if (!danger && (notices >= 2 || (last_notice[e->kind] >= 0
            && turn - last_notice[e->kind] < 50))) continue;
        dy = (int)e->y - p_ptr->py; dx = (int)e->x - p_ptr->px;
        if (ABS(dy) <= 2 && ABS(dx) <= 2) direction = "nearby";
        else if (ABS(dy) > ABS(dx)) direction = dy < 0 ? "to the north" : "to the south";
        else direction = dx < 0 ? "to the west" : "to the east";
        msg_format("You hear %s %s.", event_noise(e->kind), direction);
        if (e->kind == CAVE_EVENT_CRACK || e->kind == CAVE_EVENT_COLLAPSE)
            sound_at_environment_level(MSG_TRAP_DEADFALL, strength, e->volume);
        else if (e->kind == CAVE_EVENT_FLOOD)
            sound_at_environment_level(MSG_TRAP_FLOOD, strength, e->volume);
        else if (e->kind == CAVE_EVENT_FREEZE || e->kind == CAVE_EVENT_THAW)
            sound_at_environment_level(MSG_ICE, strength, e->volume);
        last_notice[e->kind] = turn; notices++;
    }
    (void)cave_sound_mask_at(p_ptr->py, p_ptr->px);
    bool noisy = ambient[p_ptr->py][p_ptr->px] >= 3;
    if (noisy && !in_ambient_noise && !notices && turn - ambient_notice_turn >= 200) {
        msg_print("You hear a steady rush and rumble nearby, muffling quieter sounds.");
        ambient_notice_turn = turn;
    }
    in_ambient_noise = noisy;
}
const cave_world_event* cave_events_state(void) { return events; }
u32b cave_events_next_serial(void) { return next_serial; }
void cave_events_restore(const cave_world_event* saved, u32b serial)
{
    int i;
    cave_events_reset(); memcpy(events, saved, sizeof(events));
    ambient_notice_turn = turn;
    next_serial = MAX(1, serial);
    for (i = 0; i < CAVE_EVENTS_MAX; i++)
    {
        /* Retain valid history for wizard travel; active() still prevents
         * expired events from making sound or attracting monsters. */
        if (!recorded(&events[i])) memset(&events[i], 0, sizeof(events[i]));
        announced[i] = events[i].serial;
        if (events[i].serial >= next_serial) next_serial = events[i].serial + 1;
    }
}

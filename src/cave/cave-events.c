#include "angband.h"
#include "externs.h"
#include "cave/cave-events.h"
#include "cave/cave-water-flow.h"

#define SOUND_CELLS (MAX_DUNGEON_HGT * MAX_DUNGEON_WID)
#define SOUND_LIFETIME 60
static cave_world_event events[CAVE_EVENTS_MAX];
static u32b next_serial = 1, revision = 1;
static byte fields[CAVE_EVENTS_MAX][MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static u32b field_revision[CAVE_EVENTS_MAX], field_serial[CAVE_EVENTS_MAX];
static byte ambient[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static s32b ambient_turn = -1, last_notice[CAVE_EVENT_MAX];
static s32b ambient_notice_turn = -200;
static bool in_ambient_noise;
static u32b ambient_revision, announced[CAVE_EVENTS_MAX];
static int queue[SOUND_CELLS], head, tail, count;
static bool queued[MAX_DUNGEON_HGT][MAX_DUNGEON_WID], player_moved;
static const char* event_noise(int kind);

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
    memset(last_notice, -1, sizeof(last_notice));
    next_serial = 1; revision++; ambient_turn = -1; player_moved = false;
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
int cave_sound_mask_at(int y, int x)
{
    int i, yy, xx, mask;
    if (!in_bounds(y, x)) return 0;
    if (ambient_turn != turn / 10 || ambient_revision != revision)
    {
        memset(ambient, 0, sizeof(ambient)); begin_field();
        for (yy = 0; yy < p_ptr->cur_map_hgt; yy++)
            for (xx = 0; xx < p_ptr->cur_map_wid; xx++)
            {
                int feat = cave_feat[yy][xx];
                /* Still pools, including pooled acid, are quiet. */
                if ((feat == FEAT_WATER || feat == FEAT_DEEP_WATER || feat == FEAT_POISON
                    || FEAT_IS_BRIDGE(feat))
                    && cave_water_flow_direction(yy, xx) != CAVE_WATER_FLOW_CALM)
                    ambient[yy][xx] = 8;
                else if (feat == FEAT_LAVA) ambient[yy][xx] = 5;
                if (ambient[yy][xx]) enqueue(yy, xx);
            }
        propagate(ambient); ambient_turn = turn / 10; ambient_revision = revision;
    }
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

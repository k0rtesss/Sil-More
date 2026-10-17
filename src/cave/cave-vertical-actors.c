#include "angband.h"
#include "cave/cave-vertical-actors.h"
#include "cave/cave-vertical.h"
#include "level-generation/level-cache.h"
#include "level-generation/level-state.h"
#include "monster/monster-abilities.h"
#include "monster/monster-social.h"

static int action_index;
static bool action_finished;
static u32b fall_attempt_serial;

static bool immersed_feature(int feature)
{
    return feature == FEAT_WATER || feature == FEAT_DEEP_WATER
        || feature == FEAT_LAVA || feature == FEAT_POISON;
}

static const cave_vertical_portal* forced_portal_at(int y, int x)
{
    const cave_vertical_state* state = cave_vertical_get();
    if (!state) return NULL;
    for (int i = 0; i < state->portal_count; i++)
    {
        const cave_vertical_portal* portal = &state->portals[i];
        int feature;
        if (portal->y != y || portal->x != x
            || !(portal->flags & CAVE_VERTICAL_DOWN)
            || (portal->flags & (CAVE_VERTICAL_CLOSED | CAVE_VERTICAL_GATE))
            || portal->peer_depth != p_ptr->depth + 1
            || (portal->kind != CAVE_VERTICAL_RAPIDS
                && portal->kind != CAVE_VERTICAL_FALL
                && portal->kind != CAVE_VERTICAL_LAVA_FALL
                && portal->kind != CAVE_VERTICAL_LAKE)) continue;
        feature = cave_feat[y][x];
        if (feature == FEAT_CHASM)
        {
            if (portal->kind == CAVE_VERTICAL_FALL
                || portal->kind == CAVE_VERTICAL_LAVA_FALL
                || portal->kind == CAVE_VERTICAL_LAKE) return portal;
            continue;
        }
        if (portal->kind == CAVE_VERTICAL_LAKE ? false
            : portal->material == FEAT_LAVA ? feature == FEAT_LAVA
            : portal->material == FEAT_POISON ? feature == FEAT_POISON
            : (feature == FEAT_WATER || feature == FEAT_DEEP_WATER))
            return portal;
    }
    return NULL;
}

static int available_monster_slot(const level_image* floor)
{
    for (int i = 1; i < floor->mon_max; i++)
        if (!floor->monsters[i].r_idx) return i;
    return floor->mon_max < MAX_MONSTERS ? floor->mon_max : 0;
}

static int held_object_count(const monster_type* monster)
{
    int count = 0;
    for (int i = monster->hold_o_idx; i; i = o_list[i].next_o_idx)
    {
        if (i <= 0 || i >= o_max || !o_list[i].k_idx
            || ++count >= o_max) return -1;
    }
    return count;
}

/* All capacity/geometry checks precede movement, damage and allocation. No
 * compaction or eviction is permitted in this one-actor ownership transfer. */
static level_image* transfer_destination(const cave_vertical_portal* portal,
    const monster_type* monster, int* slot)
{
    level_image* floor = level_cache_mutable_floor(portal->peer_depth);
    int capacity, held = held_object_count(monster);
    int y = portal->peer_y, x = portal->peer_x;
    if (!floor)
    {
        level_cache_request(portal->peer_depth);
        return NULL;
    }
    if (held < 0 || y <= 0 || x <= 0 || y >= floor->hgt - 1
        || x >= floor->wid - 1 || (floor->info[y][x] & CAVE_WALL)
        || floor->feat[y][x] == FEAT_CHASM || floor->monsters_at[y][x] != 0)
        return NULL;
    *slot = available_monster_slot(floor);
    if (*slot <= 0) return NULL;
    capacity = z_info->o_max - floor->o_max;
    for (int i = 1; i < floor->o_max; i++)
        if (!floor->objects[i].k_idx) capacity++;
    return held <= capacity ? floor : NULL;
}

static int destination_object_slot(level_image* floor)
{
    for (int i = 1; i < floor->o_max; i++)
        if (!floor->objects[i].k_idx) return i;
    return floor->o_max < z_info->o_max ? floor->o_max++ : 0;
}

static bool fall_into_neighbour(int index, const cave_vertical_portal* portal)
{
    cave_vertical_portal connection = *portal;
    portal = &connection;
    monster_type* source = &mon_list[index];
    monster_type* destination;
    level_image* floor;
    int slot, object_index, race_index;
    floor = transfer_destination(portal, source, &slot);
    if (!floor) return false;
    if (portal->kind == CAVE_VERTICAL_FALL
        || portal->kind == CAVE_VERTICAL_LAVA_FALL
        || portal->kind == CAVE_VERTICAL_LAKE)
    {
        /* Canonical active-map damage owns fatal drops, quest accounting and
         * special unique survival stages. Never delete a second time. */
        int who = (source->mflag & MFLAG_PLAYER_PUSH) ? -1 : 0;
        int impact = cave_vertical_fall_damage(ABS(portal->peer_depth - p_ptr->depth),
            floor->feat[portal->peer_y][portal->peer_x]);
        if (mon_take_hit(index, impact, " dies from the fall.", who))
            return true;
    }
    source = &mon_list[index];
    if (!source->r_idx) return true;
    race_index = source->r_idx;
    destination = &floor->monsters[slot];
    *destination = *source;
    destination->fy = portal->peer_y; destination->fx = portal->peer_x;
    destination->ml = false; destination->cdis = 0;
    destination->mflag &= ~(MFLAG_VIEW | MFLAG_SHOW | MFLAG_MARK
        | MFLAG_PLAYER_PUSH | MFLAG_PLAYER_KNOCKBACK);
    destination->hold_o_idx = 0;
    destination->target_y = destination->target_x = 0;
    destination->wandering_idx = 0;
    destination->song = SNG_NOTHING;
    destination->social_rival = destination->social_focus = destination->social_ally = 0;
    destination->social_state = destination->social_timer = destination->social_memory = 0;
    memset(&destination->ai, 0, sizeof(destination->ai));
    memset(&destination->squad, 0, sizeof(destination->squad));
    memset(&destination->routine, 0, sizeof(destination->routine));
    byte world_initialized = destination->world.initialized;
    byte supplies = destination->world.supplies;
    memset(&destination->world, 0, sizeof(destination->world));
    destination->world.initialized = world_initialized;
    destination->world.supplies = supplies;
    destination->world.home_y = destination->fy;
    destination->world.home_x = destination->fx;
    monster_abilities_forced_movement(destination);
    object_index = source->hold_o_idx;
    while (object_index)
    {
        int next = o_list[object_index].next_o_idx;
        int target = destination_object_slot(floor);
        /* Preflight guarantees target exists. */
        floor->objects[target] = o_list[object_index];
        floor->objects[target].held_m_idx = slot;
        floor->objects[target].iy = floor->objects[target].ix = 0;
        floor->objects[target].next_o_idx = destination->hold_o_idx;
        destination->hold_o_idx = target;
        memset(&o_list[object_index], 0, sizeof(object_type));
        floor->o_cnt++; o_cnt--; object_index = next;
    }
    if (slot >= floor->mon_max) floor->mon_max = slot + 1;
    floor->mon_cnt++;
    floor->monsters_at[portal->peer_y][portal->peer_x] = slot;
    if (r_info[race_index].flags2 & RF2_MULTIPLY)
    { num_repro--; floor->repro++; }
    /* Unique counts span resident floors. Ordinary counts own only the active
     * floor and are rebuilt when a cached image becomes active. */
    if (!(r_info[race_index].flags1 & RF1_UNIQUE)) r_info[race_index].cur_num--;
    if (p_ptr->target_who == index) target_set_monster(0);
    if (p_ptr->health_who == index) health_track(0);
    if (silent_passage_exchange_target == index) silent_passage_exchange_target = 0;
    song_disguise_handle_monster_removed(index);
    song_duels_handle_monster_removed(index);
    monster_social_remap(index, 0);
    int old_y = source->fy, old_x = source->fx;
    bool visible = source->ml;
    cave_m_idx[old_y][old_x] = 0;
    memset(source, 0, sizeof(*source)); mon_cnt--;
    lite_spot(old_y, old_x);
    if (visible) msg_print("The current carries the creature over the drop!");
    p_ptr->window |= PW_MONLIST;
    return true;
}

void cave_vertical_monster_begin_action(int index)
{
    action_index = index;
    action_finished = false;
    if (index > 0 && index < mon_max && mon_list[index].r_idx
        && !(r_info[mon_list[index].r_idx].flags2 & RF2_FLYING)
        && in_bounds_fully(mon_list[index].fy, mon_list[index].fx)
        && cave_feat[mon_list[index].fy][mon_list[index].fx] == FEAT_CHASM
        && forced_portal_at(mon_list[index].fy, mon_list[index].fx))
        mon_list[index].skip_next_turn = true;
}

bool cave_vertical_monster_fall(int index)
{
    monster_type* monster;
    const cave_vertical_portal* portal;
    if (index <= 0 || index >= mon_max) return false;
    monster = &mon_list[index];
    if (!monster->r_idx || !in_bounds_fully(monster->fy, monster->fx)
        || (r_info[monster->r_idx].flags2 & RF2_FLYING)) return false;
    portal = forced_portal_at(monster->fy, monster->fx);
    if (!portal) return false;
    fall_attempt_serial++;
    if (index == action_index) action_finished = true;
    /* A pending floor is handled too: never fall back to legacy deletion. */
    fall_into_neighbour(index, portal);
    return true;
}

void cave_vertical_monster_end_action(int index)
{
    monster_type* monster;
    const cave_vertical_portal* portal;
    int direction = 5, strength, y, x;
    if (index != action_index || action_finished) return;
    action_finished = true;
    if (index <= 0 || index >= mon_max) return;
    monster = &mon_list[index];
    if (!monster->r_idx || !in_bounds_fully(monster->fy, monster->fx)
        || (r_info[monster->r_idx].flags2 & RF2_FLYING)) return;
    portal = forced_portal_at(monster->fy, monster->fx);
    if (portal)
    {
        cave_vertical_monster_fall(index);
        return;
    }
    if (!immersed_feature(cave_feat[monster->fy][monster->fx])) return;
    strength = cave_vertical_current(monster->fy, monster->fx, &direction);
    if (strength < 2 || direction < 1 || direction > 9 || direction == 5) return;
    y = monster->fy + ddy[direction]; x = monster->fx + ddx[direction];
    if (!in_bounds_fully(y, x) || cave_m_idx[y][x] != 0
        || (p_ptr->py == y && p_ptr->px == x)
        || !cave_floor_bold(y, x) || !immersed_feature(cave_feat[y][x])) return;
    portal = forced_portal_at(y, x);
    if (portal)
    {
        int slot;
        if (!transfer_destination(portal, monster, &slot)) return;
    }
    /* The same displacement path as the player preserves entry lava/acid/
     * melting-ice hazards, illusions, facing and melee reactions. It does not
     * charge another actor action. A reaction may kill or redirect the actor. */
    u32b before_fall = fall_attempt_serial;
    monster_swap(monster->fy, monster->fx, y, x);
    monster = &mon_list[index];
    if (monster->r_idx && monster->fy == y && monster->fx == x
        && fall_attempt_serial == before_fall)
    {
        portal = forced_portal_at(y, x);
        if (portal) cave_vertical_monster_fall(index);
    }
}

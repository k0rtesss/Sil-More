#include "angband.h"
#include "externs.h"
#include "monster-world.h"
#include "monster-ai.h"
#include "monster-senses.h"
#include "cave/cave-environment.h"
#include "cave/cave-events.h"
#include "melee/melee-movement.h"

/* Small local searches, never a player flow or a global construction survey. */
#define WORLD_RADIUS 12
#define WORLD_SIDE (WORLD_RADIUS * 2 + 1)
#define WORLD_CELLS (WORLD_SIDE * WORLD_SIDE)

static bool world_hazard(monster_type* m, int feat)
{
    monster_race* r = &r_info[m->r_idx];
    if (!feat || (r->flags2 & RF2_FLYING)) return false;
    if (feat == FEAT_LAVA) return !(r->flags3 & RF3_RES_FIRE);
    if (feat == FEAT_POISON) return !(r->flags3 & RF3_RES_POIS);
    return feat == FEAT_CHASM || feat == FEAT_DEEP_WATER
        || feat == FEAT_RUBBLE;
}

static bool world_safe(monster_type* m, int y, int x)
{
    return in_bounds_fully(y, x)
        && !world_hazard(m, cave_feat[y][x])
        && !world_hazard(m, cave_environment_pending_hazard(y, x));
}

static bool world_inside(monster_type* m, int y, int x)
{
    int radius = (r_info[m->r_idx].flags2 & RF2_TERRITORIAL) ? 6 : WORLD_RADIUS;
    return distance(m->world.home_y, m->world.home_x, y, x) <= radius;
}

static void world_finish(monster_type* m)
{
    m->world.task = MON_WORLD_NONE;
    m->world.task_age = m->world.retries = 0;
    m->world.cooldown = 16;
}

void monster_world_observe(monster_type* m)
{
    monster_world_state* w = &m->world;
    monster_race* r = &r_info[m->r_idx];
    cave_world_event event;
    if (!w->initialized)
    {
        w->initialized = 1;
        w->home_y = m->fy; w->home_x = m->fx;
        w->supplies = (r->flags5 & RF5_BRIDGE_BUILDER) ? 24 : 0;
    }
    if (w->cooldown) --w->cooldown;
    if (w->observation_age && !--w->observation_age)
        w->observation_kind = CAVE_EVENT_NONE;
    if (!(r->flags2 & RF2_MINDLESS)
        && cave_event_for_listener(m->fy, m->fx, monster_skill(m, S_PER),
            w->last_event, &event))
    {
        w->last_event = event.serial;
        /* Work noise interests onlookers, but never redirects its own worker. */
        bool own_work = (event.kind == CAVE_EVENT_BUILD || event.kind == CAVE_EVENT_BRIDGE)
            && ((w->task == MON_WORLD_BRIDGE) || ((r->flags5 & RF5_BRIDGE_BUILDER)
                && distance(m->fy,m->fx,event.y,event.x) <= 1));
        if (!own_work && w->task != MON_WORLD_BRIDGE && world_inside(m, event.y, event.x))
        {
            w->observation_kind = event.kind;
            w->observation_y = event.y; w->observation_x = event.x;
            w->observation_age = 48;
            if (!w->task) w->cooldown = 0;
        }
        if (m->alertness < ALERTNESS_UNWARY)
            m->alertness = ALERTNESS_UNWARY;
    }
    if (world_hazard(m, cave_environment_pending_hazard(m->fy, m->fx))
        && m->alertness < ALERTNESS_UNWARY)
        m->alertness = ALERTNESS_UNWARY;
}

/* Claims are inferred from live occupants, so deletion/compaction cannot leave
 * dangling owner indices. Lower index wins simultaneous local claims. */
static bool world_claimed(monster_type* m, int y, int x)
{
    int i, own = (int)(m - mon_list);
    for (i = 1; i < mon_max; ++i)
    {
        monster_type* other = &mon_list[i];
        if (i == own || !other->r_idx || i > own) continue;
        if (other->world.task == MON_WORLD_BRIDGE
            && other->world.target_y == y && other->world.target_x == x
            && distance(other->fy, other->fx, y, x) <= WORLD_RADIUS
            && other->world.supplies && other->world.task_age < 48)
            return true;
    }
    return false;
}

static bool world_route_needs(monster_type* m, int y, int x)
{
    int ty = m->world.home_y, tx = m->world.home_x;
    if (m->world.observation_age)
    { ty = m->world.observation_y; tx = m->world.observation_x; }
    else if (m->world.task == MON_WORLD_INVESTIGATE)
    { ty = m->world.target_y; tx = m->world.target_x; }
    /* The monster's own home or a heard physical origin is a known destination.
     * Authored target_y can be a combat objective and must never leak here. */
    return distance(y, x, ty, tx) < distance(m->fy, m->fx, ty, tx)
        && los(m->fy, m->fx, y, x);
}

static bool world_find_job(monster_type* m)
{
    int y, x, best = 99, by = 0, bx = 0;
    environment_bridge_job job;
    if (!(r_info[m->r_idx].flags5 & RF5_BRIDGE_BUILDER) || !m->world.supplies)
        return false;
    for (y = m->fy - 4; y <= m->fy + 4; ++y)
        for (x = m->fx - 4; x <= m->fx + 4; ++x)
        {
            int d = distance(m->fy, m->fx, y, x);
            if (!in_bounds_fully(y, x) || d > 4 || d >= best
                || !world_inside(m, y, x) || !los(m->fy, m->fx, y, x)
                || !cave_environment_bridge_job_at(y, x, &job)
                || !cave_environment_job_safe(y, x) || world_claimed(m, y, x)
                || (!job.repair && !world_route_needs(m, y, x))) continue;
            best = d; by = y; bx = x;
        }
    if (!by) return false;
    if (m->world.task == MON_WORLD_INVESTIGATE && !m->world.observation_age)
    {
        m->world.observation_y = m->world.target_y;
        m->world.observation_x = m->world.target_x;
        m->world.observation_age = 48;
    }
    m->world.task = MON_WORLD_BRIDGE;
    m->world.target_y = by; m->world.target_x = bx;
    m->world.task_age = m->world.retries = 0;
    return true;
}

/* BFS over safe local terrain; returns just the first step. */
static bool world_step(monster_type* m, int ty, int tx, bool adjacent)
{
    byte seen[WORLD_SIDE][WORLD_SIDE] = {{0}};
    s16b qy[WORLD_CELLS], qx[WORLD_CELLS];
    byte first[WORLD_CELLS];
    int head = 0, tail = 1, k;
    qy[0] = m->fy; qx[0] = m->fx; first[0] = 0;
    seen[WORLD_RADIUS][WORLD_RADIUS] = 1;
    while (head < tail)
    {
        int cy = qy[head], cx = qx[head], dir = first[head++];
        if (dir && distance(cy, cx, ty, tx) <= (adjacent ? 1 : 0))
        {
            int ny = m->fy + ddy[dir], nx = m->fx + ddx[dir];
            process_move(m, ny, nx, false);
            return true;
        }
        for (k = 1; k <= 9; ++k)
        {
            int ny = cy + ddy[k], nx = cx + ddx[k];
            int sy = ny - m->fy + WORLD_RADIUS, sx = nx - m->fx + WORLD_RADIUS;
            if (k == 5 || sy < 0 || sy >= WORLD_SIDE || sx < 0 || sx >= WORLD_SIDE
                || seen[sy][sx] || !world_inside(m, ny, nx)
                || !world_safe(m, ny, nx)
                || !cave_exist_mon(&r_info[m->r_idx], ny, nx, false, false)) continue;
            seen[sy][sx] = 1;
            qy[tail] = ny; qx[tail] = nx; first[tail++] = dir ? dir : k;
        }
    }
    return false;
}

bool monster_world_turn(monster_type* m)
{
    monster_race* r = &r_info[m->r_idx];
    monster_world_state* w = &m->world;
    environment_bridge_job job;
    int k, sy, sx;
    if (r->flags1 & (RF1_PEACEFUL | RF1_NEVER_MOVE)) return false;
    if (m->confused) return false;
    /* Imminent collapse takes priority even for mindless or fighting creatures. */
    if (!world_safe(m, m->fy, m->fx))
    {
        for (k = 1; k <= 9; ++k)
        {
            int y = m->fy + ddy[k], x = m->fx + ddx[k];
            if (k != 5 && world_safe(m, y, x)
                && cave_exist_mon(r, y, x, false, false))
            { process_move(m, y, x, false); return true; }
        }
        return false;
    }
    if ((r->flags2 & RF2_MINDLESS) || m->stance == STANCE_FLEEING
        || monster_ai_can_see_player(m)
        || (m->alertness >= ALERTNESS_ALERT && monster_senses_target(m, &sy, &sx)))
    { if (w->task) world_finish(m); return false; }
    if (!w->task && !w->cooldown)
    {
        if (!world_find_job(m) && w->observation_age)
        {
            w->task = MON_WORLD_INVESTIGATE;
            w->target_y = w->observation_y; w->target_x = w->observation_x;
            w->task_age = w->retries = 0;
            w->observation_age = 0; w->observation_kind = CAVE_EVENT_NONE;
        }
        else if (!w->task) w->cooldown = 8;
    }
    if (!w->task) return false;
    if (++w->task_age > 48 || !world_inside(m, w->target_y, w->target_x))
    { world_finish(m); return false; }
    if (w->task == MON_WORLD_INVESTIGATE)
    {
        /* At an observed crossing, a builder may turn curiosity into work. */
        if (world_find_job(m)) return true;
        if (distance(m->fy, m->fx, w->target_y, w->target_x) <= 1)
        { world_finish(m); return true; }
    }
    else
    {
        if (!w->supplies || world_claimed(m, w->target_y, w->target_x)
            || !cave_environment_bridge_job_at(w->target_y, w->target_x, &job)
            || !cave_environment_job_safe(w->target_y, w->target_x))
        { world_finish(m); return false; }
        if (distance(m->fy, m->fx, w->target_y, w->target_x) <= 1)
        {
            int before = cave_environment_bridge_progress(w->target_y, w->target_x);
            bool done = cave_environment_bridge_work(w->target_y, w->target_x, job.material);
            if (done || cave_environment_bridge_progress(w->target_y, w->target_x) > before)
            {
                if (m->ml) l_list[m->r_idx].flags5 |= RF5_BRIDGE_BUILDER;
                --w->supplies; w->retries = 0;
                cave_event_emit(CAVE_EVENT_BUILD, w->target_y, w->target_x, 8);
                if (done) world_finish(m);
            }
            else if (++w->retries >= 3) world_finish(m);
            return true;
        }
    }
    if (world_step(m, w->target_y, w->target_x, true)) w->retries = 0;
    else if (++w->retries >= 3) world_finish(m);
    return true;
}

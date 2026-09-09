#include "angband.h"
#include "externs.h"
#include "melee/melee-movement-internal.h"
#include "melee/melee-util.h"

/* This is a local combat decision, not another map-wide pursuit flow. */
#define TACTICAL_RADIUS 4
#define TACTICAL_WIDTH (2 * TACTICAL_RADIUS + 1)
#define TACTICAL_CELLS (TACTICAL_WIDTH * TACTICAL_WIDTH)
#define TACTICAL_UNREACHED 100000

static bool tactical_ally(monster_type* m_ptr, int y, int x)
{
    return in_bounds(y, x) && !(y == m_ptr->fy && x == m_ptr->fx)
        && attacker_at(y, x);
}

/* Terrain is observable; the player's equipment and hidden traps are not.
 * These are priorities, not predictions of damage or resistance checks. */
static int tactical_hazard(monster_type* m_ptr, int y, int x)
{
    if (!in_bounds(y, x) || !los(m_ptr->fy, m_ptr->fx, y, x))
        return 0;
    if (cave_trap_bold(y, x) && !(cave_info[y][x] & CAVE_HIDDEN))
    {
        if (cave_pit_bold(y, x)) return 40;
        if (cave_feat[y][x] == FEAT_TRAP_WEB) return 20;
        return 30;
    }
    switch (cave_feat[y][x])
    {
    case FEAT_LAVA: return 100;
    case FEAT_CHASM: return 75;
    case FEAT_WATER: return 8;
    case FEAT_ICE: return 6;
    default: return 0;
    }
}

static bool tactical_empty_after_move(monster_type* m_ptr, int ay, int ax,
    int y, int x)
{
    if (!in_bounds(y, x) || !cave_floor_bold(y, x)
        || (y == ay && x == ax))
        return false;
    return cave_m_idx[y][x] == 0 || (y == m_ptr->fy && x == m_ptr->fx);
}

static int tactical_knockback(monster_type* m_ptr, int y, int x)
{
    int dir = rough_direction(y, x, p_ptr->py, p_ptr->px);
    int yy = p_ptr->py + ddy[dir];
    int xx = p_ptr->px + ddx[dir];
    int score = 0;
    int count = 0;

    /* Match knock_back(): an open straight destination always wins, even
     * when a side destination would be much more dangerous. */
    if (tactical_empty_after_move(m_ptr, y, x, yy, xx))
        return tactical_hazard(m_ptr, yy, xx);
    for (int side = -1; side <= 1; side += 2)
    {
        int d = cycle[chome[dir] + side];
        yy = p_ptr->py + ddy[d];
        xx = p_ptr->px + ddx[d];
        if (!tactical_empty_after_move(m_ptr, y, x, yy, xx))
            continue;
        score += tactical_hazard(m_ptr, yy, xx);
        count++;
    }
    /* The real ability randomizes which open side it tries first. */
    return count ? score / count : 0;
}

static int tactical_open_neighbors(int y, int x)
{
    int count = 0;
    for (int i = 0; i < 8; i++)
    {
        int yy = y + ddy_ddd[i];
        int xx = x + ddx_ddd[i];
        if (in_bounds(yy, xx) && cave_floor_bold(yy, xx))
            count++;
    }
    return count;
}

static int tactical_position_score(monster_type* m_ptr, int y, int x)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    int dy = y - p_ptr->py;
    int dx = x - p_ptr->px;
    int score = 0;
    int hazards = 0;
    int trailing = 0;

    for (int i = 0; i < 8; i++)
    {
        int yy = p_ptr->py + ddy_ddd[i];
        int xx = p_ptr->px + ddx_ddd[i];
        if (tactical_ally(m_ptr, yy, xx))
        {
            /* Same geometry as overwhelming_att_mod: the three opposite
             * positions give twice the support of side/front positions. */
            int dot = dy * ddy_ddd[i] + dx * ddx_ddd[i];
            score += dot < 0 ? 16 : 8;
        }
        if (tactical_hazard(m_ptr, yy, xx))
            hazards++;

        yy = m_ptr->fy + ddy_ddd[i];
        xx = m_ptr->fx + ddx_ddd[i];
        if (tactical_ally(m_ptr, yy, xx)
            && distance(yy, xx, p_ptr->py, p_ptr->px) > 1)
            trailing++;
    }

    /* Spread into open attack slots instead of crowding the lead monster. */
    for (int i = 0; i < 8; i++)
        if (tactical_ally(m_ptr, y + ddy_ddd[i], x + ddx_ddd[i]))
            score -= 3;

    /* Clear a narrow entrance when allies are waiting behind it. */
    if (trailing && tactical_open_neighbors(m_ptr->fy, m_ptr->fx) <= 4
        && tactical_open_neighbors(y, x)
            > tactical_open_neighbors(m_ptr->fy, m_ptr->fx))
        score += 18;

    if (r_ptr->flags2 & RF2_SMART)
    {
        /* Occupying a dry exit restricts retreat when other exits are
         * hazardous. Standing on a hazard instead leaves that dry exit open. */
        if (in_bounds(y, x) && cave_floor_bold(y, x)
            && !tactical_hazard(m_ptr, y, x))
            score += MIN(16, hazards * 4);
    }
    if (r_ptr->flags2 & RF2_KNOCK_BACK)
        score += tactical_knockback(m_ptr, y, x);

    return score - monster_terrain_penalty(m_ptr, y, x) * 10;
}

static bool tactical_enterable(monster_type* m_ptr, int y, int x)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    bool bash = false;
    if (!cave_exist_mon(r_ptr, y, x, false, false))
        return false;
    if ((r_ptr->flags1 & RF1_HIDDEN_MOVE)
        && ((cave_info[y][x] & CAVE_SEEN) || seen_by_keen_senses(y, x)))
        return false;
    /* An elaborate flank is never worth deliberately crossing burning lava.
     * Monsters already on it may still choose any safe escape step. */
    if (monster_terrain_penalty(m_ptr, y, x) >= 100)
        return false;
    /* Repositioning should be an immediate step, not digging, opening a
     * door, or attempting to displace an ally. */
    return cave_passable_mon(m_ptr, y, x, &bash) >= 100;
}

bool get_move_tactical(monster_type* m_ptr, int* ty, int* tx)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    int cost[TACTICAL_CELLS];
    int turns[TACTICAL_CELLS];
    int first[TACTICAL_CELLS];
    bool done[TACTICAL_CELLS] = { false };
    int py = p_ptr->py;
    int px = p_ptr->px;
    int oy = py - TACTICAL_RADIUS;
    int ox = px - TACTICAL_RADIUS;
    int dist = distance(m_ptr->fy, m_ptr->fx, py, px);
    int best_score = -TACTICAL_UNREACHED;
    int best_first = -1;
    bool adjacent = dist <= 1;
    int start;

    if (m_ptr->confused || m_ptr->alertness < ALERTNESS_ALERT
        || m_ptr->stance == STANCE_FLEEING || m_ptr->min_range > 1
        || (r_ptr->flags2 & RF2_MINDLESS)
        || (r_ptr->flags1 & (RF1_NEVER_MOVE | RF1_NEVER_BLOW | RF1_PEACEFUL))
        || dist > 3 || !player_has_los_bold(m_ptr->fy, m_ptr->fx)
        || !((r_ptr->flags2 & (RF2_SMART | RF2_KNOCK_BACK | RF2_FLANKING))
            || (r_ptr->flags1 & (RF1_FRIEND | RF1_FRIENDS))))
        return false;

    if (adjacent)
        best_score = tactical_position_score(m_ptr, m_ptr->fy, m_ptr->fx)
            + ((r_ptr->flags2 & RF2_FLANKING) ? 2 : 6);

    for (int i = 0; i < TACTICAL_CELLS; i++)
    {
        cost[i] = TACTICAL_UNREACHED;
        turns[i] = TACTICAL_UNREACHED;
        first[i] = -1;
    }
    start = (m_ptr->fy - oy) * TACTICAL_WIDTH + m_ptr->fx - ox;
    cost[start] = 0;
    turns[start] = 0;

    /* Dijkstra retains the first legal step to each slot. A greedy target
     * behind an ally or wall must never cause a detour into the player. */
    for (int visit = 0; visit < TACTICAL_CELLS; visit++)
    {
        int at = -1;
        for (int i = 0; i < TACTICAL_CELLS; i++)
            if (!done[i] && cost[i] < TACTICAL_UNREACHED
                && (at < 0 || cost[i] < cost[at]))
                at = i;
        if (at < 0)
            break;
        done[at] = true;
        int y = oy + at / TACTICAL_WIDTH;
        int x = ox + at % TACTICAL_WIDTH;

        if (at != start && distance(y, x, py, px) == 1)
        {
            int score = tactical_position_score(m_ptr, y, x) - cost[at];
            if (score > best_score)
            {
                best_score = score;
                best_first = first[at];
            }
        }
        /* Adjacent monsters consider only one-step improvements, so they
         * never leave melee for a speculative trip around the player. */
        if ((adjacent && at != start) || turns[at] >= TACTICAL_RADIUS)
            continue;
        for (int i = 0; i < 8; i++)
        {
            int yy = y + ddy_ddd[i];
            int xx = x + ddx_ddd[i];
            int ry = yy - oy;
            int rx = xx - ox;
            if (ry < 0 || ry >= TACTICAL_WIDTH || rx < 0 || rx >= TACTICAL_WIDTH
                || !tactical_enterable(m_ptr, yy, xx))
                continue;
            int next = ry * TACTICAL_WIDTH + rx;
            int step = monster_step_cost(m_ptr, y, x, yy, xx);
            int next_turns = turns[at] + step;
            /* Include hazards along the route, not just at its eventual
             * attack slot: flying through lava still burns the monster. */
            int next_cost = cost[at] + step * 4
                + monster_terrain_penalty(m_ptr, yy, xx);
            if (step <= 0 || next_turns > TACTICAL_RADIUS
                || next_cost >= cost[next])
                continue;
            cost[next] = next_cost;
            turns[next] = next_turns;
            first[next] = at == start ? next : first[at];
        }
    }

    if (best_first < 0)
        return false;
    *ty = oy + best_first / TACTICAL_WIDTH;
    *tx = ox + best_first % TACTICAL_WIDTH;
    return true;
}

#include "angband.h"
#include "externs.h"
#include "monster/monster-routine.h"
#include "cave/cave-environment.h"
#include "melee/melee-util.h"

#define ROUTINE_RADIUS 16
#define ROUTINE_SIDE (2 * ROUTINE_RADIUS + 1)
#define ROUTINE_CELLS (ROUTINE_SIDE * ROUTINE_SIDE)

bool monster_routine_allows(const monster_type* m, int y, int x)
{
    int home = m->routine.territory - 1;
    /* Forced displacement can put an actor outside; allow its return journey. */
    return home < 0 || level_partition_index_for_point(y, x) == home
        || level_partition_index_for_point(m->fy, m->fx) != home;
}

static bool routine_cell(monster_type* m, int y, int x)
{
    if (!in_bounds_fully(y, x) || !monster_routine_allows(m, y, x)
        || cave_trap_bold(y, x) || cave_glyph(y, x)
        || cave_environment_pending_hazard(y, x)
        || monster_terrain_penalty(m, y, x)
        || (cave_info[y][x] & CAVE_ICKY) !=
            (cave_info[m->routine.home_y][m->routine.home_x] & CAVE_ICKY))
        return false;
    /* Circuits use existing passages, never tunnelling or wall phasing. */
    if (cave_any_closed_door_bold(y, x))
        return (r_info[m->r_idx].flags2 & (RF2_OPEN_DOOR | RF2_PASS_DOOR)) != 0;
    return !cave_monster_wall_bold(y, x)
        && cave_exist_mon(&r_info[m->r_idx], y, x, true, false);
}

/* Reject arbitrary laps across an empty room. A circuit must enclose an
 * obstacle (a pillar, wall island, or vault) in the cardinal passage graph. */
static bool encloses_obstacle(const int* path, int count, const bool* pass)
{
    for (int cell = 0; cell < ROUTINE_CELLS; cell++)
    {
        if (pass[cell]) continue;
        int cy = cell / ROUTINE_SIDE, cx = cell % ROUTINE_SIDE;
        bool inside = false;
        for (int i = 0, j = count - 1; i < count; j = i++)
        {
            int iy = path[i] / ROUTINE_SIDE, ix = path[i] % ROUTINE_SIDE;
            int jy = path[j] / ROUTINE_SIDE;
            if ((iy > cy) != (jy > cy) && ix > cx) inside = !inside;
        }
        if (inside) return true;
    }
    return false;
}

bool monster_routine_plan(monster_type* m)
{
    monster_routine_state* r = &m->routine;
    int parent[ROUTINE_CELLS], depth[ROUTINE_CELLS], queue[ROUTINE_CELLS];
    bool pass[ROUTINE_CELLS];
    int y0 = r->home_y - ROUTINE_RADIUS, x0 = r->home_x - ROUTINE_RADIUS;
    int root = ROUTINE_RADIUS * ROUTINE_SIDE + ROUTINE_RADIUS;
    int head = 0, tail = 0;
    r->style = MON_ROUTINE_NONE; r->count = r->next = 0;
    for (int i = 0; i < ROUTINE_CELLS; i++)
    {
        parent[i] = -1; depth[i] = 0;
        pass[i] = routine_cell(m, y0 + i / ROUTINE_SIDE, x0 + i % ROUTINE_SIDE);
    }
    if (!pass[root]) return false;
    parent[root] = root; queue[tail++] = root;
    while (head < tail)
    {
        int a = queue[head++], ay = a / ROUTINE_SIDE, ax = a % ROUTINE_SIDE;
        const int dy[4] = {-1, 0, 1, 0}, dx[4] = {0, 1, 0, -1};
        for (int d = 0; d < 4; d++)
        {
            int by = ay + dy[d], bx = ax + dx[d];
            if (by < 0 || bx < 0 || by >= ROUTINE_SIDE || bx >= ROUTINE_SIDE) continue;
            int b = by * ROUTINE_SIDE + bx;
            if (!pass[b]) continue;
            if (parent[b] < 0)
            {
                parent[b] = a; depth[b] = depth[a] + 1; queue[tail++] = b;
                continue;
            }
            if (parent[a] == b || parent[b] == a) continue;
            int left[MON_PATROL_MAX], right[MON_PATROL_MAX], nl = 0, nr = 0;
            int u = a, v = b;
            while (u != v && nl + nr < MON_PATROL_MAX - 1)
            {
                if (depth[u] >= depth[v]) { left[nl++] = u; u = parent[u]; }
                else { right[nr++] = v; v = parent[v]; }
            }
            if (u != v) continue;
            left[nl++] = u;
            while (nr) left[nl++] = right[--nr];
            if (nl < 12 || nl <= r->count || !encloses_obstacle(left, nl, pass)) continue;
            r->count = nl;
            for (int i = 0; i < nl; i++)
            { r->y[i] = y0 + left[i] / ROUTINE_SIDE; r->x[i] = x0 + left[i] % ROUTINE_SIDE; }
        }
    }
    if (!r->count) return false;
    /* Join at the closest point, then keep this direction and route forever. */
    for (int i = 1; i < r->count; i++)
        if (distance(m->fy, m->fx, r->y[i], r->x[i])
            < distance(m->fy, m->fx, r->y[r->next], r->x[r->next])) r->next = i;
    r->style = MON_ROUTINE_PATROL;
    return true;
}

void monster_routine_spawn(monster_type* m)
{
    monster_routine_state* s = &m->routine;
    const monster_race* r = &r_info[m->r_idx];
    memset(s, 0, sizeof(*s));
    s->home_y = m->fy; s->home_x = m->fx;
    if (p_ptr->depth <= 0 || p_ptr->game_type < 0 || m->r_idx == R_IDX_MORGOTH
        || (r->flags1 & RF1_PEACEFUL)
        || m->r_idx == R_IDX_HUMAN_THRALL || m->r_idx == R_IDX_ELF_THRALL) return;
    int kind = level_partition_kind_for_point(m->fy, m->fx);
    int pi = level_partition_index_for_point(m->fy, m->fx);
    if (pi >= 0 && (kind == LEVEL_PART_BIG_CAVE || kind == LEVEL_PART_CHASM))
        s->territory = pi + 1;
    if (!s->territory && !(r->flags1 & (RF1_NEVER_MOVE | RF1_HIDDEN_MOVE | RF1_UNIQUE))
        && !(r->flags2 & (RF2_TERRITORIAL | RF2_MINDLESS | RF2_SHORT_SIGHTED))
        && one_in_(4))
    {
        s->style = MON_ROUTINE_PENDING;
        if (character_dungeon) monster_routine_plan(m);
    }
}

void monster_routine_finish_level(void)
{
    for (int i = 1; i < mon_max; i++)
        if (mon_list[i].r_idx && mon_list[i].routine.style == MON_ROUTINE_PENDING)
            monster_routine_plan(&mon_list[i]);
}

static bool return_step(monster_type* m, int target_y, int target_x, int* y, int* x)
{
    /* Search the same safe passage graph as the patrol. A combat flow can
     * shortcut through a vault, hazard or diggable wall and strand the actor
     * when its first step is rejected by the routine's stricter rules. */
    byte seen[MAX_DUNGEON_HGT][MAX_DUNGEON_WID] = {{0}};
    byte qy[4096], qx[4096], first[4096];
    int head = 0, tail = 1;
    qy[0] = m->fy; qx[0] = m->fx; first[0] = 0;
    seen[m->fy][m->fx] = 1;
    while (head < tail)
    {
        int cy = qy[head], cx = qx[head], direction = first[head++];
        for (int d = 0; d < 8; d++)
        {
            int yy = cy + ddy_ddd[d], xx = cx + ddx_ddd[d];
            if (!in_bounds_fully(yy, xx) || seen[yy][xx]) continue;
            seen[yy][xx] = 1;
            if (!routine_cell(m, yy, xx) || cave_m_idx[yy][xx]
                || !monster_step_cost(m, cy, cx, yy, xx)) continue;
            int step = direction ? direction : d + 1;
            if (yy == target_y && xx == target_x)
            {
                *y = m->fy + ddy_ddd[step - 1];
                *x = m->fx + ddx_ddd[step - 1];
                return true;
            }
            if (tail == 4096) continue;
            qy[tail] = yy; qx[tail] = xx; first[tail++] = step;
        }
    }
    return false;
}

bool monster_routine_move(monster_type* m, int* y, int* x)
{
    monster_routine_state* r = &m->routine;
    *y = m->fy; *x = m->fx;
    if (p_ptr->truce || p_ptr->depth == 0
        || (r_info[m->r_idx].flags1 & (RF1_NEVER_MOVE | RF1_HIDDEN_MOVE))) return false;
    if (r->territory)
    {
        if (level_partition_index_for_point(m->fy, m->fx) != r->territory - 1)
            return return_step(m, r->home_y, r->home_x, y, x);
        /* Lair-bound races retain their original hoard behavior. */
        if (r_info[m->r_idx].flags2 & RF2_TERRITORIAL) return false;
        if (!one_in_(4)) return false;
        int d = rand_int(8), yy = m->fy + ddy_ddd[d], xx = m->fx + ddx_ddd[d];
        if (!routine_cell(m, yy, xx) || cave_m_idx[yy][xx]) return false;
        *y = yy; *x = xx; return true;
    }
    if (r->style != MON_ROUTINE_PATROL || !r->count) return false;
    if (m->fy == r->y[r->next] && m->fx == r->x[r->next])
        r->next = (r->next + 1) % r->count;
    int yy = r->y[r->next], xx = r->x[r->next];
    if (!routine_cell(m, yy, xx))
    {
        /* A permanently changed passage retires this circuit; no new random
         * route silently replaces an individual monster's original patrol. */
        r->style = MON_ROUTINE_NONE; r->count = r->next = 0;
        return false;
    }
    if (distance(m->fy, m->fx, yy, xx) <= 1)
    {
        if (cave_m_idx[yy][xx]) return false;
        *y = yy; *x = xx; return true;
    }
    return return_step(m, yy, xx, y, x);
}

bool monster_routine_valid(const monster_type* m)
{
    const monster_routine_state* r = &m->routine;
    if (r->territory > PARTITION_META_MAX || r->style > MON_ROUTINE_PATROL
        || r->count > MON_PATROL_MAX || (r->count && r->next >= r->count)
        || (!r->count && r->next) || (r->territory && r->style)
        || (r->style == MON_ROUTINE_PATROL ? r->count < 12 : r->count != 0)) return false;
    if ((r->territory || r->style) && !in_bounds_fully(r->home_y, r->home_x)) return false;
    if (r->territory && level_partition_index_for_point(r->home_y, r->home_x)
        != r->territory - 1) return false;
    for (int i = 0; i < r->count; i++)
    {
        int j = (i + 1) % r->count;
        if (!in_bounds_fully(r->y[i], r->x[i])
            || ABS(r->y[i] - r->y[j]) + ABS(r->x[i] - r->x[j]) != 1) return false;
        for (int k = 0; k < i; k++)
            if (r->y[k] == r->y[i] && r->x[k] == r->x[i]) return false;
    }
    return true;
}

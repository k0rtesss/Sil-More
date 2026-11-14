#include "angband.h"
#include "externs.h"
#include "monster/monster-senses.h"
#include "monster/monster-routine.h"
#include "melee/melee-util.h"

/* These are sensory rules, shared by perception and AI. Player UI visibility
 * and monster lore deliberately have no part in recognition. */
int monster_sight_range(const monster_type* m_ptr)
{
    if (!m_ptr || !m_ptr->r_idx || m_ptr->alertness < ALERTNESS_UNWARY)
        return 0;
    const monster_race* r_ptr = &r_info[m_ptr->r_idx];
    int range = (r_ptr->flags2 & RF2_SHORT_SIGHTED) ? 2 : MAX_SIGHT;
    if (!monster_race_is_vala(m_ptr->r_idx) && visual_recognition
        && (r_ptr->flags2 & RF2_SMART))
    {
        int divisor = p_ptr->active_ability[S_STL][STL_DISGUISE] ? 4 : 2;
        int vision = monster_skill((monster_type*)m_ptr, S_PER) / divisor
            + p_ptr->cur_light
            + ((cave_info[p_ptr->py][p_ptr->px] & CAVE_GLOW) ? 2 : 0);
        range = MIN(range, MAX(0, vision));
    }
    return range;
}

bool monster_has_sight(const monster_type* m_ptr)
{
    int range = monster_sight_range(m_ptr);
    return range > 0
        && distance(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px) <= range
        && los(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px);
}

int monster_scent_limit(const monster_race* r_ptr)
{
    /* The old C/f mapping includes named hounds and cats. Explicit family
     * flags also cover dragons whose glyph differs; serpents never acquire it. */
    if (r_ptr->flags3 & RF3_SERPENT)
        return 0;
    if ((r_ptr->flags3 & (RF3_WOLF | RF3_DRAGON)) || r_ptr->d_char == 'C')
        return 80;
    if ((r_ptr->flags3 & RF3_CAT) || r_ptr->d_char == 'f')
        return 40;
    return 0;
}

static bool usable_scent(const monster_type* m_ptr, int y, int x, int* age)
{
    int limit = monster_scent_limit(&r_info[m_ptr->r_idx]);
    if (!limit)
    {
        *age = -1;
        return false;
    }
    *age = get_scent(y, x);
    return limit && *age >= 0 && *age <= limit;
}

static void remember(monster_type* m_ptr, int kind, int y, int x)
{
    monster_sense_state* s = &m_ptr->ai.sense;
    s->kind = kind;
    s->y = s->anchor_y = y;
    s->x = s->anchor_x = x;
    s->observed_turn = playerturn;
    s->scent_age = 255;
    s->stale_decisions = s->search_decisions = 0;
    s->recent_count = s->recent_next = 0;
    m_ptr->mflag |= MFLAG_ACTV;
}

void monster_senses_see(monster_type* m_ptr, int y, int x)
{
    if (in_bounds(y, x))
        remember(m_ptr, MON_SENSE_SIGHT, y, x);
}

void monster_senses_hear(monster_type* m_ptr, int y, int x)
{
    if (in_bounds(y, x) && !(m_ptr->ai.sense.kind == MON_SENSE_SIGHT
            && m_ptr->ai.sense.observed_turn == (u32b)playerturn))
        remember(m_ptr, MON_SENSE_SOUND, y, x);
}

void monster_senses_share_trace(monster_type* m_ptr, int y, int x)
{
    const monster_sense_state* s = &m_ptr->ai.sense;
    if (monster_has_sight(m_ptr)
        || ((s->kind == MON_SENSE_SIGHT || s->kind == MON_SENSE_SOUND)
            && s->observed_turn == (u32b)playerturn))
        return;
    if (in_bounds(y, x))
        remember(m_ptr, MON_SENSE_SHARED_TRACE, y, x);
}

/* Compare trace timestamps, not just their aging byte. Re-reading the same
 * trace after the player acts cannot renew a stalled pursuit. */
static bool fresher_trace(const monster_sense_state* s, int age)
{
    if (s->scent_age == 255 || (!s->anchor_y && !s->anchor_x))
        return true;
    u32b elapsed = (u32b)playerturn - s->observed_turn;
    return elapsed <= 1000000U && age < (int)s->scent_age + (int)elapsed;
}

void monster_senses_refresh(monster_type* m_ptr)
{
    monster_sense_state* s = &m_ptr->ai.sense;
    int age;
    if (!m_ptr->r_idx || m_ptr->alertness < ALERTNESS_UNWARY
        || m_ptr->r_idx == R_IDX_MORGOTH)
        return;
    if (monster_has_sight(m_ptr))
    {
        monster_senses_see(m_ptr, p_ptr->py, p_ptr->px);
        return;
    }
    if ((s->kind == MON_SENSE_SOUND || s->kind == MON_SENSE_SIGHT)
        && s->observed_turn == (u32b)playerturn)
        return;
    if (!usable_scent(m_ptr, m_ptr->fy, m_ptr->fx, &age))
        return;
    if (s->kind == MON_SENSE_SCENT && !fresher_trace(s, age))
    {
        /* A plateau still confirms the occupied trace location, but does not
         * grant any more pursuit decisions or erase the recent-cell history. */
        s->anchor_y = s->y = m_ptr->fy;
        s->anchor_x = s->x = m_ptr->fx;
        return;
    }
    if ((s->kind == MON_SENSE_NONE || s->kind == MON_SENSE_SEARCH)
        && !fresher_trace(s, age))
        return;
    remember(m_ptr, MON_SENSE_SCENT, m_ptr->fy, m_ptr->fx);
    s->scent_age = age;
}

bool monster_senses_target(const monster_type* m_ptr, int* y, int* x)
{
    const monster_sense_state* s = &m_ptr->ai.sense;
    if (s->kind == MON_SENSE_NONE || !in_bounds(s->y, s->x))
        return false;
    *y = s->y;
    *x = s->x;
    return true;
}

bool monster_senses_tracking_scent(const monster_type* m_ptr)
{
    if (!m_ptr || !m_ptr->r_idx)
        return false;
    const monster_sense_state* s = &m_ptr->ai.sense;
    return monster_scent_limit(&r_info[m_ptr->r_idx])
        && s->scent_age != 255
        && (s->kind == MON_SENSE_SCENT || s->kind == MON_SENSE_SEARCH);
}

/* A dragon can investigate an acquired trail locally without abandoning its
 * hoard or acquiring an unseen player's current position. Other trackers keep
 * their existing range. Displaced actors must return through the usual route. */
static bool scent_lair_cell_allowed(const monster_type* m_ptr, int y, int x)
{
    const monster_race* r = &r_info[m_ptr->r_idx];
    if (!(r->flags2 & RF2_TERRITORIAL) || !(r->flags3 & RF3_DRAGON))
        return true;
    int hy = m_ptr->routine.home_y, hx = m_ptr->routine.home_x;
    if (!in_bounds_fully(hy, hx) || !in_bounds_fully(y, x)
        || distance(hy, hx, y, x) > MONSTER_LAIR_RADIUS
        || (cave_info[y][x] & CAVE_ICKY) != (cave_info[hy][hx] & CAVE_ICKY))
        return false;
    if (m_ptr->routine.territory
        && level_partition_index_for_point(m_ptr->fy, m_ptr->fx)
            != m_ptr->routine.territory - 1)
        return false;
    return monster_routine_allows(m_ptr, y, x);
}

static bool recent_cell(const monster_sense_state* s, int y, int x)
{
    for (int i = 0; i < s->recent_count && i < 4; i++)
        if (s->recent_y[i] == y && s->recent_x[i] == x)
            return true;
    return false;
}

static void remember_cell(monster_sense_state* s, int y, int x)
{
    s->recent_y[s->recent_next % 4] = y;
    s->recent_x[s->recent_next % 4] = x;
    s->recent_next = (s->recent_next + 1) % 4;
    if (s->recent_count < 4)
        s->recent_count++;
}

bool monster_senses_advance(monster_type* m_ptr, int* ty, int* tx)
{
    monster_sense_state* s = &m_ptr->ai.sense;
    int my = m_ptr->fy, mx = m_ptr->fx;
    int current_age, best_age = 256, best_cost = FLOW_MAX_DIST;
    bool found = false;
    *ty = my; *tx = mx;
    if (!s->kind)
        return false;
    if (monster_senses_tracking_scent(m_ptr)
        && !scent_lair_cell_allowed(m_ptr, my, mx))
        return false;

    if (s->kind == MON_SENSE_SCENT)
    {
        /* Stalled pursuit is bounded even on a large equal-age plateau. */
        if (s->stale_decisions < 8
            && usable_scent(m_ptr, my, mx, &current_age))
        {
            for (int d = 0; d < 8; d++)
            {
                int y = my + ddy_ddd[d], x = mx + ddx_ddd[d], age;
                if (!in_bounds(y, x) || !scent_lair_cell_allowed(m_ptr, y, x)
                    || cave_m_idx[y][x] < 0
                    || !usable_scent(m_ptr, y, x, &age) || age > current_age
                    || (age == current_age && recent_cell(s, y, x)))
                    continue;
                int cost = monster_step_cost(m_ptr, my, mx, y, x);
                if (!cost || age > best_age
                    || (age == best_age && cost >= best_cost))
                    continue;
                found = true; best_age = age; best_cost = cost;
                *ty = y; *tx = x;
            }
            s->stale_decisions++;
            if (found)
            {
                remember_cell(s, my, mx);
                return true;
            }
        }
        s->kind = MON_SENSE_SEARCH;
        s->y = s->anchor_y; s->x = s->anchor_x;
    }
    else if (s->kind != MON_SENSE_SEARCH)
    {
        if (s->y != my || s->x != mx)
        {
            *ty = s->y; *tx = s->x;
            return true;
        }
        s->kind = MON_SENSE_SEARCH;
        s->anchor_y = my; s->anchor_x = mx;
    }

    if (s->search_decisions >= 8)
    {
        s->kind = MON_SENSE_NONE;
        return false;
    }
    s->search_decisions++;
    /* Search is actual local movement, never a scan for distant fresh scent. */
    for (int d = 0; d < 8; d++)
    {
        int y = my + ddy_ddd[d], x = mx + ddx_ddd[d];
        if (!in_bounds(y, x)
            || (monster_senses_tracking_scent(m_ptr)
                && !scent_lair_cell_allowed(m_ptr, y, x))
            || cave_m_idx[y][x] < 0
            || distance(y, x, s->anchor_y, s->anchor_x) > 2
            || recent_cell(s, y, x))
            continue;
        int cost = monster_step_cost(m_ptr, my, mx, y, x);
        if (!cost || cost >= best_cost)
            continue;
        best_cost = cost; found = true; *ty = y; *tx = x;
    }
    remember_cell(s, my, mx);
    return found;
}

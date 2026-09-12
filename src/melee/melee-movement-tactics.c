#include "angband.h"
#include "externs.h"
#include "melee/melee-movement-internal.h"
#include "melee/melee-util.h"
#include "monster/monster-abilities.h"
#include "monster/monster-ai.h"
#include "monster/monster-tactics.h"
#include "monster/monster-senses.h"
#include "cave/cave-light.h"
#include "melee/melee-movement-morgoth.h"

/* Actor-centred search, at most one additional own action. Three bounded
 * non-dominated labels retain a safe alternative when local routes merge. */
#define TACTICAL_RADIUS 4
#define TACTICAL_WIDTH 9
#define TACTICAL_CELLS 81
#define TACTICAL_LABELS 3
#define TACTICAL_STEPS 2

typedef struct tactical_label
{
    int cost, damage, pending, steps, first;
    bool used, done;
} tactical_label;

typedef struct tactical_context
{
    monster_type* actor;
    monster_type* allies[TACTICAL_CELLS];
    int ally_count, oy, ox, py, px, original_distance;
    bool ranged, shadow, sight;
} tactical_context;

typedef struct tactical_choice
{
    int y, x, goal_y, goal_x, gain;
} tactical_choice;

static int tactical_confidence(const monster_type* m_ptr, int feature)
{
    int value = monster_ai_confidence(m_ptr, feature);
    if (r_info[m_ptr->r_idx].flags2 & RF2_SMART) return value;
    return value > 0 ? (value + 1) / 2 : (value - 1) / 2;
}

static bool tactical_ally(monster_type* m_ptr, int y, int x)
{
    return in_bounds(y, x) && !(y == m_ptr->fy && x == m_ptr->fx)
        && attacker_at(y, x) && los(m_ptr->fy, m_ptr->fx, y, x);
}

/* Terrain is observable; hidden traps and player equipment are not. */
static int tactical_hazard(monster_type* m_ptr, int y, int x)
{
    int score, defense = 0;
    if (!in_bounds(y, x) || !los(m_ptr->fy, m_ptr->fx, y, x)) return 0;
    if (cave_trap_bold(y, x) && !(cave_info[y][x] & CAVE_HIDDEN))
    {
        if (cave_pit_bold(y, x)) return 40;
        if (cave_feat[y][x] == FEAT_TRAP_WEB) return 20;
        return 30;
    }
    switch (cave_feat[y][x])
    {
    case FEAT_LAVA:
        score = 100; defense = tactical_confidence(m_ptr, MON_AI_FIRE); break;
    case FEAT_POISON:
        score = 20; defense = tactical_confidence(m_ptr, MON_AI_POISON); break;
    case FEAT_CHASM: return 75;
    case FEAT_WATER: return 8;
    case FEAT_ICE: return 6; /* Cold resistance is not sure footing. */
    default: return 0;
    }
    return score * (4 - MAX(0, defense)) / 4;
}

static bool tactical_empty_after_move(monster_type* m_ptr, int ay, int ax,
    int y, int x)
{
    if (!in_bounds(y, x) || !cave_floor_bold(y, x) || (y == ay && x == ax)) return false;
    return cave_m_idx[y][x] == 0 || (y == m_ptr->fy && x == m_ptr->fx);
}

static int tactical_knockback_path(monster_type* m_ptr, int ay, int ax,
    int y, int x, int dy, int dx, int max_distance, int* dest_y, int* dest_x)
{
    int distance = 0;
    for (int step = 1; step <= max_distance; ++step)
    {
        int yy = y + step * dy, xx = x + step * dx;
        if (!tactical_empty_after_move(m_ptr, ay, ax, yy, xx)) break;
        *dest_y = yy; *dest_x = xx; distance = step;
    }
    return distance;
}

int monster_tactical_displacement_utility(monster_type* m_ptr, int y, int x,
    bool exchange)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    int dir, yy, xx, score = 0, count = 0;
    int knock_distance = (!p_ptr->leaping
        && cave_feat[p_ptr->py][p_ptr->px] == FEAT_ICE)
        ? ICE_KNOCK_BACK_DISTANCE : 1;
    if (!monster_ai_can_see_player(m_ptr)
        || distance(y, x, p_ptr->py, p_ptr->px) != 1) return 0;
    if (exchange)
    {
        if (!(r_ptr->flags2 & RF2_EXCHANGE_PLACES)) return 0;
        /* Exchange always offers a normal opportunity attack, distinct from
         * learned Opportunist and Zone of Control. */
        return tactical_hazard(m_ptr, y, x) - 12;
    }
    if (!(r_ptr->flags2 & RF2_KNOCK_BACK)) return 0;
    dir = rough_direction(y, x, p_ptr->py, p_ptr->px);
    /* Execution tries the straight destination before randomized sides. */
    if (tactical_knockback_path(m_ptr, y, x, p_ptr->py, p_ptr->px,
            ddy[dir], ddx[dir], knock_distance, &yy, &xx) > 0)
        return tactical_hazard(m_ptr, yy, xx);
    for (int side = -1; side <= 1; side += 2)
    {
        int d = cycle[chome[dir] + side];
        if (tactical_knockback_path(m_ptr, y, x, p_ptr->py, p_ptr->px,
                ddy[d], ddx[d], knock_distance, &yy, &xx) == 0)
            continue;
        score += tactical_hazard(m_ptr, yy, xx);
        count++;
    }
    return count ? score / count : 0;
}

static int tactical_knockback(monster_type* m_ptr, int y, int x)
{
    return monster_tactical_displacement_utility(m_ptr, y, x, false);
}

static int tactical_open_neighbors(int y, int x)
{
    int count = 0;
    for (int i = 0; i < 8; i++)
    {
        int yy = y + ddy_ddd[i], xx = x + ddx_ddd[i];
        if (in_bounds(yy, xx) && cave_floor_bold(yy, xx)) count++;
    }
    return count;
}

static int tactical_shadow_score(const tactical_context* c, int y, int x)
{
    monster_type* m_ptr = c->actor;
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    bool glow = (r_ptr->flags2 & RF2_GLOW) != 0;
    int score = 0, player_before = 0, player_after = 0;
    if (!c->shadow) return 0;
    int old_player = cave_light_contribution(r_ptr->light, glow,
        m_ptr->fy, m_ptr->fx, c->py, c->px);
    int new_player = cave_light_contribution(r_ptr->light, glow,
        y, x, c->py, c->px);
    bool player_known = cave_light_observed_change(m_ptr, c->py, c->px,
        old_player, new_player, &player_before, &player_after);
    for (int i = 0; i < c->ally_count; ++i)
    {
        monster_type* ally = c->allies[i];
        monster_race* race = &r_info[ally->r_idx];
        int before, after;
        int engagement = distance(ally->fy, ally->fx, c->py, c->px);
        if (engagement > 4 || ally->alertness < ALERTNESS_ALERT) continue;
        int old = cave_light_contribution(r_ptr->light, glow,
            m_ptr->fy, m_ptr->fx, ally->fy, ally->fx);
        int next = cave_light_contribution(r_ptr->light, glow,
            y, x, ally->fy, ally->fx);
        if (!cave_light_observed_change(m_ptr, ally->fy, ally->fx,
                old, next, &before, &after)) continue;
        /* Losing actual illumination provides modest cover even to allies
         * without light aversion. Other player senses remain uncertain. */
        if (before > 0 && after <= 0) score += 4;
        if (before <= 0 && after > 0) score -= 4;
        if (race->flags3 & RF3_HURT_LITE)
        {
            /* Actual attack/evasion penalty at the ally, and morale penalty
             * facing a brightly lit player. No reward below thresholds. */
            score += (MAX(0, before - 2) - MAX(0, after - 2))
                * (engagement == 1 ? 8 : 4);
            if (player_known)
                score += (MAX(0, player_before - 3) - MAX(0, player_after - 3)) * 2;
        }
        if (player_known && race->light > 0 && strchr("@G", race->d_char))
        {
            /* These actual combat profiles lose positive attack/evasion
             * when they cannot illuminate the player. */
            if (player_before > 0 && player_after <= 0) score -= 24;
            if (player_before <= 0 && player_after > 0) score += 24;
        }
    }
    return score;
}

static bool tactical_on_lane(int y, int x, int sy, int sx, int ty, int tx)
{
    int dy = ty - sy, dx = tx - sx, yy = y - sy, xx = x - sx;
    return (yy || xx) && yy * dx == xx * dy && yy * dy + xx * dx > 0
        && distance(sy, sx, y, x) < distance(sy, sx, ty, tx);
}

static int tactical_reaction_cost(const tactical_context* c,
    int from_y, int from_x, int y, int x)
{
    monster_type* m_ptr = c->actor;
    int old = distance(from_y, from_x, c->py, c->px);
    int next = distance(y, x, c->py, c->px), cost = 0;
    if (old == 1 && next == 1)
        cost += MAX(0, tactical_confidence(m_ptr, MON_AI_ZONE)) * 8;
    if (old == 1 && next > 1)
        cost += MAX(0, tactical_confidence(m_ptr, MON_AI_OPPORTUNIST)) * 8;
    if (old > 1 && next == 1)
        cost += MAX(0, tactical_confidence(m_ptr, MON_AI_POLEARM)) * 6;
    return cost;
}

/* Impale reaches exactly the first two squares along one of eight directions.
 * Both the front creature and the one behind it should avoid forming the pair. */
static bool tactical_impale_pair(int py, int px, int y, int x, int ay, int ax)
{
    int dy = y - py, dx = x - px;
    int ady = ay - py, adx = ax - px;
    return ((MAX(ABS(dy), ABS(dx)) == 1 && ady == 2 * dy && adx == 2 * dx)
        || (MAX(ABS(ady), ABS(adx)) == 1 && dy == 2 * ady && dx == 2 * adx));
}

static int tactical_position_score(const tactical_context* c, int y, int x)
{
    monster_type* m_ptr = c->actor;
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    int dist = distance(y, x, c->py, c->px);
    bool moved = y != m_ptr->fy || x != m_ptr->fx;
    int score = c->ranged ? 42 - ABS(dist - m_ptr->best_range) * 9 : 56 - dist * 8;
    int trailing = 0;
    int impale = MAX(0, tactical_confidence(m_ptr, MON_AI_IMPALE));
    int sweep = MAX(0, tactical_confidence(m_ptr, MON_AI_WHIRLWIND));
    int follow = MAX(0, tactical_confidence(m_ptr, MON_AI_FOLLOW_THROUGH));
    bool has_shot = !c->ranged || projectable(y, x, c->py, c->px, PROJECT_CHCK);
    if (!c->ranged && dist == 1) score += 8;
    if (c->ranged && !has_shot) score -= 24;
    if (c->ranged && dist < m_ptr->min_range) score -= 15;
    for (int i = 0; i < c->ally_count; ++i)
    {
        monster_type* ally = c->allies[i];
        int ally_dist = distance(ally->fy, ally->fx, c->py, c->px);
        int separation = distance(y, x, ally->fy, ally->fx);
        if (dist == 1 && ally_dist == 1)
        {
            int dot = (y - c->py) * (ally->fy - c->py) + (x - c->px) * (ally->fx - c->px);
            score += dot < 0 ? 16 : 8;
            /* Sweeps reach the entire ring, including opposite sides.
             * Follow Through needs a kill first; a wounded ally makes that
             * opening more likely. Neither depends on inter-ally adjacency. */
            int follow_risk = ally->hp <= ally->maxhp / 2 ? 8 : 3;
            score -= MAX(sweep * 9, follow * follow_risk);
            if (!dot) score += MAX(0, tactical_confidence(m_ptr, MON_AI_FLANKING)) * 2;
        }
        if (distance(m_ptr->fy, m_ptr->fx, ally->fy, ally->fx) == 1 && ally_dist > 1) trailing++;
        if (separation == 1) score -= 3;
        if (impale && tactical_impale_pair(c->py, c->px, y, x, ally->fy, ally->fx))
            score -= impale * 8;
        if (r_info[ally->r_idx].freq_ranged
            && tactical_on_lane(y, x, ally->fy, ally->fx, c->py, c->px)) score -= 18;
        if (ally->stance == STANCE_FLEEING
            && tactical_on_lane(y, x, c->py, c->px, ally->fy, ally->fx)) score -= 10;
    }
    if (trailing && tactical_open_neighbors(m_ptr->fy, m_ptr->fx) <= 4
        && tactical_open_neighbors(y, x) > tactical_open_neighbors(m_ptr->fy, m_ptr->fx)) score += 18;
    if (dist == 1)
    {
        score += tactical_knockback(m_ptr, y, x) / 2;
        if (!moved || (c->original_distance == 1
                && (r_ptr->flags2 & RF2_FLANKING) && monster_abilities_can_react(m_ptr)))
            score += monster_vengeance_bonus_dice_preview(m_ptr) * 8;
        if (!moved)
        {
            score += monster_concentration_bonus_preview(m_ptr, true) * 5;
            if ((r_ptr->flags2 & RF2_ZONE_OF_CONTROL) && monster_abilities_can_react(m_ptr)) score += 10;
        }
    }
    if (!moved && dist <= MAX(2, m_ptr->best_range))
        score += monster_blocking_bonus_dice_preview(m_ptr) * 5;
    if (moved && dist <= MAX(2, m_ptr->best_range))
    {
        if (r_ptr->flags5 & RF5_DODGING) score += 6;
        if (c->original_distance == 1 && dist == 1 && (r_ptr->flags2 & RF2_FLANKING)
            && monster_abilities_can_react(m_ptr)) score += 2;
    }
    /* Preserve a useful directional run, not a circular charge-up route. */
    if (moved && (r_ptr->flags5 & RF5_SPRINTING) && dist < c->original_distance)
    {
        int first = m_ptr->ability_in_action ? 1 : 0;
        int prior = m_ptr->previous_action[first];
        int dir = rough_direction(m_ptr->fy, m_ptr->fx, y, x);
        if (prior >= 1 && prior <= 9 && prior != 5
            && ddy[prior] * ddy[dir] + ddx[prior] * ddx[dir] > 0)
            score += monster_sprinting_preview(m_ptr) ? 8 : 4;
    }
    if (moved)
    {
        int action = m_ptr->ai.player_action;
        bool recent_move = action >= 1 && action <= 9 && action != 5
            && playerturn - m_ptr->ai.player_action_turn <= 1;
        if (recent_move && dist <= 3)
        {
            int forward = (y - c->py) * ddy[action] + (x - c->px) * ddx[action];
            int kiting = MAX(0, tactical_confidence(m_ptr, MON_AI_KITING));
            int retreat = MAX(0, tactical_confidence(m_ptr, MON_AI_CONTROLLED_RETREAT));
            /* Take reachable junctions ahead of a witnessed retreating route
             * instead of blindly repeating its stand/retreat/chase sequence. */
            if (forward > 0) score += kiting * 3;
            if (forward < 0 && dist == 1) score -= retreat * 4;
            if (forward > 0 && dist == 2)
                score -= MAX(0, tactical_confidence(m_ptr, MON_AI_CHARGE)) * 5;
        }
        if (dist == 1)
        {
            int dir = rough_direction(c->py, c->px, y, x);
            int yy = y + ddy[dir], xx = x + ddx[dir];
            if (in_bounds(yy, xx) && cave_floor_bold(yy, xx)
                && !cave_m_idx[yy][xx])
            {
                int hazard = monster_terrain_penalty(m_ptr, yy, xx);
                if (cave_feat[yy][xx] == FEAT_CHASM && !(r_ptr->flags2 & RF2_FLYING)) hazard = 100;
                score -= MAX(0, tactical_confidence(m_ptr, MON_AI_KNOCKBACK)) * MIN(12, hazard);
            }
            score -= MAX(0, tactical_confidence(m_ptr, MON_AI_EXCHANGE))
                * MIN(8, monster_terrain_penalty(m_ptr, c->py, c->px));
            if (m_ptr->ai.attack_y == m_ptr->fy && m_ptr->ai.attack_x == m_ptr->fx)
                score += MAX(0, tactical_confidence(m_ptr, MON_AI_CONCENTRATION)) * 4;
        }
        /* A prepared opponent is a reason to improve cover or use existing
         * ranged pressure; waiting does not erase their Concentration. */
        if (c->ranged && has_shot && dist > 1)
            score += MAX(0, tactical_confidence(m_ptr, MON_AI_FOCUS)) * 2;
    }
    if (moved && y == m_ptr->ai.previous_y && x == m_ptr->ai.previous_x) score -= 18;
    if (m_ptr->ai.goal_age && y == m_ptr->ai.goal_y && x == m_ptr->ai.goal_x) score += 5;
    score += tactical_shadow_score(c, y, x);
    if ((r_ptr->flags3 & RF3_HURT_LITE) || c->shadow)
    {
        int before, after;
        bool glow = (r_ptr->flags2 & RF2_GLOW) != 0;
        int old = cave_light_contribution(r_ptr->light, glow, m_ptr->fy, m_ptr->fx, y, x);
        int next = cave_light_contribution(r_ptr->light, glow, y, x, y, x);
        if (cave_light_observed_change(m_ptr, y, x, old, next, &before, &after))
        {
            if (r_ptr->flags3 & RF3_HURT_LITE) score -= MAX(0, after - 2) * 8;
            if (after <= 0) score += 4;
        }
    }
    return score - monster_terrain_penalty(m_ptr, y, x) * 10;
}

static bool tactical_enterable(monster_type* m_ptr, int y, int x)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    bool bash = false;
    if (!in_bounds(y, x) || !cave_exist_mon(r_ptr, y, x, false, false)) return false;
    if ((r_ptr->flags1 & RF1_HIDDEN_MOVE)
        && ((cave_info[y][x] & CAVE_SEEN) || seen_by_keen_senses(y, x))) return false;
    if (monster_terrain_penalty(m_ptr, y, x) >= 100) return false;
    return cave_passable_mon(m_ptr, y, x, &bash) >= 100;
}

/* A cheap but poisoned label cannot erase a survivable alternative. */
static void tactical_add_label(tactical_label labels[TACTICAL_LABELS], tactical_label next)
{
    int slot = -1;
    for (int i = 0; i < TACTICAL_LABELS; ++i)
    {
        if (!labels[i].used) { slot = i; continue; }
        if (labels[i].cost <= next.cost && labels[i].damage <= next.damage
            && labels[i].pending <= next.pending && labels[i].steps <= next.steps) return;
    }
    for (int i = 0; i < TACTICAL_LABELS; ++i)
        if (labels[i].used && next.cost <= labels[i].cost && next.damage <= labels[i].damage
            && next.pending <= labels[i].pending && next.steps <= labels[i].steps)
        { labels[i].used = false; slot = i; }
    if (slot < 0)
    {
        int safest = 0, worst = -1;
        for (int i = 1; i < TACTICAL_LABELS; ++i)
            if (labels[i].damage + labels[i].pending < labels[safest].damage + labels[safest].pending) safest = i;
        for (int i = 0; i < TACTICAL_LABELS; ++i)
            if (i != safest && (worst < 0 || labels[i].cost > labels[worst].cost)) worst = i;
        if (next.cost >= labels[worst].cost
            && next.damage + next.pending >= labels[safest].damage + labels[safest].pending) return;
        slot = worst;
    }
    labels[slot] = next;
}

static bool tactical_choose(monster_type* m_ptr, tactical_choice* choice)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    tactical_context c = { 0 };
    tactical_label labels[TACTICAL_CELLS][TACTICAL_LABELS] = { 0 };
    int start = TACTICAL_RADIUS * TACTICAL_WIDTH + TACTICAL_RADIUS;
    int baseline, best_score, best_first = -1, best_goal = -1;
    bool adjacent, avoid_sweep;
    if (!monster_ai_enabled(m_ptr)
        || m_ptr->confused || m_ptr->alertness < ALERTNESS_ALERT || m_ptr->stance == STANCE_FLEEING
        || (r_ptr->flags1 & (RF1_NEVER_MOVE | RF1_PEACEFUL))) return false;
    c.shadow = (r_ptr->flags2 & RF2_SMART) && r_ptr->light < 0;
    c.sight = monster_ai_can_see_player(m_ptr);
    c.actor = m_ptr;
    if (c.sight) { c.py = p_ptr->py; c.px = p_ptr->px; }
    else if (!c.shadow || !monster_senses_target(m_ptr, &c.py, &c.px)) return false;
    c.oy = m_ptr->fy - TACTICAL_RADIUS; c.ox = m_ptr->fx - TACTICAL_RADIUS;
    c.original_distance = distance(m_ptr->fy, m_ptr->fx, c.py, c.px);
    if (c.original_distance > MAX(6, m_ptr->best_range + 1)) return false;
    c.ranged = m_ptr->min_range > 1 || (r_ptr->flags1 & RF1_NEVER_BLOW);
    adjacent = c.original_distance == 1;
    avoid_sweep = tactical_confidence(m_ptr, MON_AI_WHIRLWIND) > 0
        || tactical_confidence(m_ptr, MON_AI_FOLLOW_THROUGH) > 0;
    for (int i = 0; i < TACTICAL_CELLS; ++i)
    {
        int y = c.oy + i / TACTICAL_WIDTH, x = c.ox + i % TACTICAL_WIDTH;
        if (tactical_ally(m_ptr, y, x)) c.allies[c.ally_count++] = &mon_list[cave_m_idx[y][x]];
    }
    baseline = tactical_position_score(&c, m_ptr->fy, m_ptr->fx)
        - monster_ai_poison_damage(m_ptr, m_ptr->fy, m_ptr->fx, 1) * 3;
    best_score = baseline + ((r_ptr->flags2 & RF2_FLANKING) ? 3 : 6);
    labels[start][0] = (tactical_label){ .pending = m_ptr->poisoned, .first = -1, .used = true };
    for (int visit = 0; visit < TACTICAL_CELLS * TACTICAL_LABELS; ++visit)
    {
        int at = -1, which = -1;
        for (int i = 0; i < TACTICAL_CELLS; ++i)
            for (int n = 0; n < TACTICAL_LABELS; ++n)
                if (labels[i][n].used && !labels[i][n].done
                    && (at < 0 || labels[i][n].cost < labels[at][which].cost)) { at = i; which = n; }
        if (at < 0) break;
        labels[at][which].done = true;
        tactical_label here = labels[at][which];
        int y = c.oy + at / TACTICAL_WIDTH, x = c.ox + at % TACTICAL_WIDTH;
        int dist = distance(y, x, c.py, c.px);
        if (at != start && (!adjacent || dist == 1 || c.ranged || c.shadow
                || (avoid_sweep && dist == 2)))
        {
            int score = tactical_position_score(&c, y, x) - here.cost - here.damage * 3;
            /* Small deterministic variation does not consume game RNG. */
            score += (at + m_ptr->r_idx + m_ptr->fy + m_ptr->fx) % 3;
            if (score > best_score) { best_score = score; best_first = here.first; best_goal = at; }
        }
        if (here.steps >= TACTICAL_STEPS || (adjacent && at != start)) continue;
        for (int d = 0; d < 8; ++d)
        {
            int yy = y + ddy_ddd[d], xx = x + ddx_ddd[d];
            int ry = yy - c.oy, rx = xx - c.ox;
            if (ry < 0 || ry >= TACTICAL_WIDTH || rx < 0 || rx >= TACTICAL_WIDTH
                || !tactical_enterable(m_ptr, yy, xx)) continue;
            int next_cell = ry * TACTICAL_WIDTH + rx;
            if (next_cell == start) continue;
            bool pool = cave_feat[yy][xx] == FEAT_POISON
                && !(r_ptr->flags2 & RF2_FLYING) && !(r_ptr->flags3 & RF3_RES_POIS);
            tactical_label next = here;
            next.used = true; next.done = false; next.steps++;
            next.first = at == start ? next_cell : here.first;
            next.damage += monster_ai_poison_preview(here.pending, pool,
                cave_feat[y][x] != FEAT_POISON, 1, &next.pending);
            if (next.damage >= m_ptr->hp) continue;
            /* Already doomed monsters can still seek an exit. */
            if (pool && here.damage + here.pending < m_ptr->hp
                && next.damage + next.pending >= m_ptr->hp) continue;
            next.cost += water_movement_energy(100, cave_feat[y][x], cave_feat[yy][xx],
                (r_ptr->flags2 & RF2_FLYING) != 0) / 25;
            next.cost += tactical_reaction_cost(&c, y, x, yy, xx);
            next.cost += monster_terrain_penalty(m_ptr, yy, xx);
            tactical_add_label(labels[next_cell], next);
        }
    }
    if (best_first < 0) return false;
    choice->y = c.oy + best_first / TACTICAL_WIDTH;
    choice->x = c.ox + best_first % TACTICAL_WIDTH;
    choice->goal_y = c.oy + best_goal / TACTICAL_WIDTH;
    choice->goal_x = c.ox + best_goal % TACTICAL_WIDTH;
    choice->gain = best_score - baseline;
    return true;
}

int monster_tactical_move_utility(monster_type* m_ptr)
{
    tactical_choice choice;
    return tactical_choose(m_ptr, &choice) ? choice.gain : 0;
}

bool get_move_tactical(monster_type* m_ptr, int* ty, int* tx)
{
    tactical_choice choice;
    if (m_ptr->r_idx == R_IDX_MORGOTH)
        return get_move_tactical_morgoth(m_ptr, ty, tx);
    if (!tactical_choose(m_ptr, &choice)) return false;
    *ty = choice.y; *tx = choice.x;
    /* Only execution commits. Reusing an anchor cannot refresh its budget. */
    if (!m_ptr->ai.goal_age || m_ptr->ai.goal_y != choice.goal_y || m_ptr->ai.goal_x != choice.goal_x)
    {
        m_ptr->ai.goal_y = choice.goal_y; m_ptr->ai.goal_x = choice.goal_x;
        m_ptr->ai.goal_age = 3;
    }
    return true;
}

#include "angband.h"
#include "externs.h"
#include "melee/melee-movement-internal.h"
#include "melee/melee-util.h"
#include "monster/monster-abilities.h"
#include "monster/monster-ai.h"
#include "monster/monster-social.h"
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
    bool ranged, shadow, sight, commanded, pack;
} tactical_context;

typedef struct tactical_choice
{
    int y, x, goal_y, goal_x, gain;
    int reason;
} tactical_choice;

static int squad_position_score(const tactical_context* c, int y, int x);
static int squad_job_score(const monster_type* m, int y, int x);

static int tactical_confidence(const monster_type* m_ptr, int feature)
{
    int value = monster_ai_confidence(m_ptr, feature);
    if (r_info[m_ptr->r_idx].flags2 & RF2_SMART) return value;
    return value > 0 ? (value + 1) / 2 : (value - 1) / 2;
}

static bool tactical_ally(monster_type* m_ptr, int y, int x)
{
    if (!in_bounds(y, x) || (y == m_ptr->fy && x == m_ptr->fx)
        || cave_m_idx[y][x] <= 0 || !los(m_ptr->fy, m_ptr->fx, y, x))
        return false;
    return monster_social_allies(m_ptr, &mon_list[cave_m_idx[y][x]]);
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
    case FEAT_DEEP_WATER: return 60; /* This is the player's landing hazard. */
    case FEAT_ICE:
    case FEAT_MELTING_ICE: return 6; /* Cold resistance is not sure footing. */
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
        && FEAT_IS_ICE(cave_feat[p_ptr->py][p_ptr->px]))
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

/* Prefer a useful fighting position on terrain that hinders the opponent
 * without harming us. Merely being allowed onto a square is not protection:
 * flight avoids pools and poor footing, but does not prevent lava damage.
 * Keep the reward bounded so it cannot outweigh pursuit or a lost firing lane. */
static int tactical_protected_terrain(const tactical_context* c, int y, int x,
    int dist, bool has_shot)
{
    monster_type* m_ptr = c->actor;
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    bool flying = (r_ptr->flags2 & RF2_FLYING) != 0;
    bool protected = false;
    if (!c->sight || !has_shot
        || (c->ranged ? (dist < m_ptr->min_range || dist > m_ptr->best_range)
                      : dist != 1))
        return 0;
    switch (cave_feat[y][x])
    {
    case FEAT_LAVA: protected = (r_ptr->flags3 & RF3_RES_FIRE) != 0; break;
    case FEAT_POISON: protected = flying || (r_ptr->flags3 & RF3_RES_POIS); break;
    case FEAT_CHASM:
    case FEAT_WATER:
    case FEAT_DEEP_WATER:
    case FEAT_ICE:
    case FEAT_MELTING_ICE: protected = flying; break;
    default: break;
    }
    if (!protected) return 0;
    return MIN(24, tactical_hazard(m_ptr, y, x) / 2);
}

/* A healthy melee creature can screen one nearby ally. Use a maximum rather
 * than adding every ally's reward, and leave an already occupied guard post
 * to its defender. All allies here were locally seen and affiliation-checked. */
static int tactical_guard_score_visible(const tactical_context* c, int y, int x,
    bool withdrawal, bool visible_only)
{
    monster_type* m = c->actor;
    if (!c->sight || c->ranged || m->hp * 2 <= m->maxhp
        || (r_info[m->r_idx].flags1 & RF1_NEVER_BLOW)) return 0;
    int dist = distance(y, x, c->py, c->px), best = 0;
    if (dist > 2 || !los(y, x, c->py, c->px)) return 0;
    for (int i = 0; i < c->ally_count; ++i)
    {
        monster_type* ally = c->allies[i];
        if (visible_only && !ally->ml) continue;
        int ally_dist = distance(ally->fy, ally->fx, c->py, c->px);
        if (ally->alertness < ALERTNESS_ALERT || ally_dist <= dist || ally_dist > 5
            || distance(y, x, ally->fy, ally->fx) > 2) continue;
        if (withdrawal)
        {
            if (ally->stance != STANCE_FLEEING || ally->hp * 2 > ally->maxhp)
                continue;
        }
        else
        {
            if (!r_info[ally->r_idx].freq_ranged || ally->stance == STANCE_FLEEING
                || ally_dist < 2
                || !los(ally->fy, ally->fx, c->py, c->px)
                || tactical_on_lane(y, x, ally->fy, ally->fx, c->py, c->px))
                continue;
        }
        bool covered = false;
        for (int j = 0; j < c->ally_count; ++j)
        {
            monster_type* guard = c->allies[j];
            if (guard == ally || guard->stance == STANCE_FLEEING
                || guard->alertness < ALERTNESS_ALERT || guard->hp * 2 <= guard->maxhp
                || guard->min_range > 1
                || (r_info[guard->r_idx].flags1 & RF1_NEVER_BLOW)) continue;
            int guard_dist = distance(guard->fy, guard->fx, c->py, c->px);
            if (guard_dist <= 2 && guard_dist < ally_dist
                && distance(guard->fy, guard->fx, ally->fy, ally->fx) <= 2
                && los(guard->fy, guard->fx, c->py, c->px)
                && (withdrawal || !tactical_on_lane(guard->fy, guard->fx,
                    ally->fy, ally->fx, c->py, c->px)))
            { covered = true; break; }
        }
        if (!covered) best = MAX(best, withdrawal ? 30 : 24);
    }
    return best;
}

static int tactical_guard_score(const tactical_context* c, int y, int x,
    bool withdrawal)
{
    return tactical_guard_score_visible(c, y, x, withdrawal, false);
}

/* Hear the current performance; generic remembered singing is not evidence
 * of a particular song. Sound distance, not LOS alone, weakens these effects.
 * No player song skill, Voice, or target index enters this preview. */
static void tactical_song_scores(const tactical_context* c, int y, int x,
    bool has_shot, int* spacing, int* pressure)
{
    monster_type* m = c->actor;
    int current_noise, next_noise, weight = 0;
    *spacing = *pressure = 0;
    if (!c->sight) return;
    current_noise = flow_dist(FLOW_PLAYER_NOISE, m->fy, m->fx);
    if (current_noise > MAX_SIGHT) return;
    next_noise = flow_dist(FLOW_PLAYER_NOISE, y, x);
    if (next_noise >= FLOW_MAX_DIST) return;
    if (!(singing(SNG_CHALLENGE) && m->stance == STANCE_AGGRESSIVE))
    {
        /* Ranged creatures can give up an attack to buy a few squares of
         * resistance. Melee creatures keep their useful attack contact.
         * Six sound steps is a tactical spacing limit, not a song radius. */
        if (singing(SNG_MASTERY)) weight = c->ranged ? 24 : 4;
        if (singing(SNG_LORIEN)) weight = MAX(weight, c->ranged ? 20 : 3);
        if (singing(SNG_ELBERETH)
            && (r_info[m->r_idx].flags2 & RF2_SMART)) weight = MAX(weight, 3);
        if (has_shot)
            *spacing = MAX(-24, MIN(24,
                weight * (MIN(6, next_noise) - MIN(6, current_noise))));
    }
    /* A duel has no escape radius after selection. React only to our own
     * still-active pressure stacks, not the player's hidden selected target. */
    bool contest = singing(SNG_CONTEST) && m->song_contest_stacks
        && !m->song_contest_completed;
    bool lament = singing(SNG_LAMENT) && m->song_lament_stacks
        && !m->song_lament_completed;
    if (contest || lament)
    {
        int dist = distance(y, x, c->py, c->px);
        if (c->ranged)
            *pressure = has_shot && dist >= m->min_range && dist <= m->best_range ? 24 : 0;
        else
            *pressure = dist == 1 ? 24 : dist == 2 ? 8 : 0;
    }
}

static int tactical_position_details(const tactical_context* c, int y, int x,
    int reasons[MON_TACTIC_MAX])
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
    int parts[MON_TACTIC_MAX] = { 0 };
    if (c->commanded) parts[MON_TACTIC_ORDER] = squad_position_score(c, y, x);
    parts[MON_TACTIC_TERRAIN] = tactical_protected_terrain(c, y, x, dist, has_shot);
    if (!c->pack)
    {
        parts[MON_TACTIC_GUARD] = tactical_guard_score(c, y, x, false);
        parts[MON_TACTIC_WITHDRAWAL] = tactical_guard_score(c, y, x, true);
    }
    tactical_song_scores(c, y, x, has_shot,
        &parts[MON_TACTIC_SONG_DISTANCE], &parts[MON_TACTIC_SONG_PRESSURE]);
    /* One screening job, even when both kinds of ally are nearby. */
    if (parts[MON_TACTIC_WITHDRAWAL]) parts[MON_TACTIC_GUARD] = 0;
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
            parts[MON_TACTIC_SWEEP] -= MAX(sweep * 9, follow * follow_risk);
            if (!dot) score += MAX(0, tactical_confidence(m_ptr, MON_AI_FLANKING)) * 2;
        }
        if (distance(m_ptr->fy, m_ptr->fx, ally->fy, ally->fx) == 1 && ally_dist > 1) trailing++;
        if (separation == 1) score -= 3;
        if (impale && tactical_impale_pair(c->py, c->px, y, x, ally->fy, ally->fx))
            parts[MON_TACTIC_IMPALE] -= impale * 8;
        if (r_info[ally->r_idx].freq_ranged
            && tactical_on_lane(y, x, ally->fy, ally->fx, c->py, c->px)) score -= 18;
        if (ally->stance == STANCE_FLEEING
            && tactical_on_lane(ally->fy, ally->fx, c->py, c->px, y, x)) score -= 10;
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
            if (forward > 0) parts[MON_TACTIC_INTERCEPT] += kiting * 3;
            if (forward < 0 && dist == 1) parts[MON_TACTIC_INTERCEPT] -= retreat * 4;
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
                parts[MON_TACTIC_CONCENTRATION] += MAX(0, tactical_confidence(m_ptr, MON_AI_CONCENTRATION)) * 4;
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
    for (int i = 1; i < MON_TACTIC_MAX; ++i) score += parts[i];
    if (reasons) memcpy(reasons, parts, sizeof(parts));
    return score - monster_terrain_penalty(m_ptr, y, x) * 10;
}

static int tactical_position_score(const tactical_context* c, int y, int x)
{
    return tactical_position_details(c, y, x, NULL);
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

/* A commander plans a small, visible squad once per scheduler pass. These
 * reservations are shared by every member, rather than independently guessed
 * from the positions left by monsters that happened to act first. */
#define SQUAD_RADIUS 6
#define SQUAD_MEMBERS 8
#define SQUAD_MAX 16

static bool squad_available(const monster_type* m)
{
    return !p_ptr->truce && monster_ai_enabled(m) && m->hp > 0
        && m->alertness >= ALERTNESS_ALERT && !m->confused && !m->stunned
        && !m->skip_this_turn && !m->skip_next_turn && !m->smite_recovery
        && m->stance != STANCE_FLEEING && m->min_range < FLEE_RANGE
        && !m->social_state && !song_disguise_monster_is_fooled(m)
        && !(r_info[m->r_idx].flags1 & (RF1_PEACEFUL | RF1_NEVER_MOVE));
}

static int squad_command_rank(const monster_type* m)
{
    const monster_race* r = &r_info[m->r_idx];
    if (!r->command_grade || !r->command_kin) return 0;
    /* Level only breaks ties within a deliberately authored leadership grade. */
    return r->command_grade * 1024 + r->level;
}

static bool squad_authority(const monster_type* leader, const monster_type* m)
{
    const monster_race* r = &r_info[leader->r_idx];
    const monster_race* other = &r_info[m->r_idx];
    if (!other->command_kin || other->command_kin >= MON_KIN_MAX
        || !(r->command_authority & (1UL << other->command_kin))
        || monster_social_relation(leader, m) == MON_REL_HOSTILE) return false;
    if (r->command_style == MON_COMMAND_PACK)
        return r->command_kin == other->command_kin
            && other->command_style == MON_COMMAND_PACK
            && monster_social_allies(leader, m);
    /* Custom allegiances and personal feuds take precedence over race command. */
    if (leader->social_group || m->social_group)
        return monster_social_allies(leader, m);
    if (r->command_kin == other->command_kin) return monster_social_allies(leader, m);
    /* A shared wandering band alone never grants cross-species command. */
    return r->command_grade == MON_COMMAND_COMMANDER;
}

static bool squad_contact(const monster_type* leader, const monster_type* m)
{
    int radius = ((r_info[leader->r_idx].flags2 | r_info[m->r_idx].flags2)
        & RF2_SHORT_SIGHTED) ? 2 : SQUAD_RADIUS;
    return squad_authority(leader, m)
        && distance(leader->fy, leader->fx, m->fy, m->fx) <= radius
        && los(leader->fy, leader->fx, m->fy, m->fx);
}

bool monster_squad_order_valid(const monster_type* m)
{
    int idx = m ? m->squad.commander : 0;
    if (idx <= 0 || idx >= mon_max || !squad_available(m)
        || !m->squad.role || !m->squad.job
        || !in_bounds_fully(m->squad.y, m->squad.x)) return false;
    const monster_type* leader = &mon_list[idx];
    if (!squad_available(leader) || !squad_command_rank(leader)
        || !squad_contact(leader, m) || !monster_ai_can_see_player(leader)) return false;
    /* An old order must never track a player who has moved out of sight. */
    return m->squad.target_y == p_ptr->py && m->squad.target_x == p_ptr->px;
}

static bool squad_ranged(const monster_type* m)
{
    const monster_race* r = &r_info[m->r_idx];
    return m->min_range > 1 || (r->flags1 & RF1_NEVER_BLOW)
        || (r->freq_ranged && (r->flags4 & RF4_ARCHERY_MASK));
}

/* Reserve the corridor containing a discretized shot, including shallow
 * diagonals. Exact collinearity misses cells on those firing paths. */
static bool squad_on_lane(int y, int x, int sy, int sx, int ty, int tx)
{
    int dy = ty - sy, dx = tx - sx, yy = y - sy, xx = x - sx;
    return (yy || xx) && ABS(yy * dx - xx * dy) <= MAX(ABS(dy), ABS(dx))
        && yy * dy + xx * dx > 0
        && distance(sy, sx, y, x) < distance(sy, sx, ty, tx);
}

bool monster_squad_hold_position(const monster_type* m)
{
    bool post = m->squad.job == MON_JOB_HOLD || m->squad.job == MON_JOB_REGROUP
        || m->squad.job == MON_JOB_GUARD || m->squad.job == MON_JOB_COVER;
    return monster_squad_order_valid(m) && (squad_ranged(m) || post)
        && m->fy == m->squad.y && m->fx == m->squad.x
        && distance(m->fy, m->fx, m->squad.target_y, m->squad.target_x) >= 2
        && !monster_terrain_penalty((monster_type*)m, m->fy, m->fx)
        && monster_ai_poison_safe(m, m->fy, m->fx, 2)
        && (post || projectable(m->fy, m->fx, m->squad.target_y, m->squad.target_x, PROJECT_CHCK));
}

static int squad_position_score(const tactical_context* c, int y, int x)
{
    const monster_squad_order* order = &c->actor->squad;
    int score = MAX(-48, 24 - 16 * distance(y, x, order->y, order->x))
        + squad_job_score(c->actor, y, x);
    for (int i = 0; i < c->ally_count; ++i)
    {
        const monster_type* ally = c->allies[i];
        if (ally->squad.commander != order->commander || !ally->squad.role) continue;
        if (y == ally->squad.y && x == ally->squad.x) score -= 40;
        if (squad_ranged(ally) && squad_on_lane(y, x, ally->squad.y,
                ally->squad.x, c->py, c->px)) score -= 24;
    }
    return score;
}

/* Empty, observed, legal routes only: no plans through an ally, a wall the
 * commander cannot see beyond, or a door that still needs opening. */
static bool squad_enterable(monster_type* m, const monster_type* leader, int y, int x)
{
    return in_bounds_fully(y, x)
        && (!cave_m_idx[y][x] || (y == m->fy && x == m->fx))
        && (los(m->fy, m->fx, y, x) || los(leader->fy, leader->fx, y, x))
        && tactical_enterable(m, y, x)
        && monster_ai_poison_safe(m, y, x, TACTICAL_RADIUS);
}

static int squad_job_score(const monster_type* m, int y, int x)
{
    const monster_squad_order* o = &m->squad;
    int dist = distance(y, x, o->target_y, o->target_x);
    int anchor_dist = distance(y, x, o->anchor_y, o->anchor_x);
    int anchor_target = distance(o->anchor_y, o->anchor_x, o->target_y, o->target_x);
    switch (o->job)
    {
    case MON_JOB_FLANK:
        return dist == 1 && (y - o->target_y) * (o->anchor_y - o->target_y)
            + (x - o->target_x) * (o->anchor_x - o->target_x) < 0 ? 28 : 0;
    case MON_JOB_GUARD:
    case MON_JOB_COVER:
        return anchor_dist <= 2 && dist < anchor_target
            && !squad_on_lane(y, x, o->anchor_y, o->anchor_x, o->target_y, o->target_x)
            ? (o->job == MON_JOB_COVER ? 44 : 36) : -anchor_dist * 4;
    case MON_JOB_HOLD:
        return -anchor_dist * 20 - (dist < 2 ? 48 : 0);
    case MON_JOB_REGROUP:
        return MIN(6, dist) * 40 - anchor_dist * 8;
    default: return 0;
    }
}

static void squad_assign_jobs(monster_type* leader, monster_type** members, int count,
    const monster_squad_order* previous)
{
    const monster_race* r = &r_info[leader->r_idx];
    int injured = 0, near = MAX_SIGHT, far = 0, ranged = -1, front = -1;
    for (int i = 0; i < count; ++i)
    {
        monster_type* m = members[i];
        int dist = distance(m->fy, m->fx, p_ptr->py, p_ptr->px);
        /* Rear-line archers and a commander covering them are deliberately
         * spaced back. Only assemble the approaching melee soldiers. */
        if (m != leader && !squad_ranged(m))
        { near = MIN(near, dist); far = MAX(far, dist); }
        if (m->hp * 2 <= m->maxhp) ++injured;
        if (squad_ranged(m)) ranged = i;
        else if (front < 0 || dist < distance(members[front]->fy, members[front]->fx,
                p_ptr->py, p_ptr->px)) front = i;
        m->squad.job = MON_JOB_ADVANCE;
    }
    bool tactical = r->command_style == MON_COMMAND_TACTICAL;
    int plan = MON_PLAN_ADVANCE;
    if (tactical && r->command_grade == MON_COMMAND_COMMANDER)
    {
        if (injured * 2 >= count) plan = MON_PLAN_REGROUP;
        else if (near >= 4 && far - near >= 3) plan = MON_PLAN_HOLD;
    }
    /* At most three player turns of regrouping / two of assembly. A persistent
     * condition cannot restart its own budget after orders return to advance. */
    int age = 0;
    if (plan != MON_PLAN_ADVANCE && previous->role == MON_SQUAD_COMMANDER
        && (previous->plan != MON_PLAN_ADVANCE || previous->plan_age))
        age = MIN(8, previous->plan_age + (previous->plan_turn != (u32b)playerturn));
    if (age >= (plan == MON_PLAN_HOLD ? 2 : 3)) plan = MON_PLAN_ADVANCE;
    for (int i = 0; i < count; ++i)
    {
        monster_squad_order* o = &members[i]->squad;
        o->plan = plan; o->plan_age = age; o->plan_turn = playerturn;
        o->anchor_y = leader->fy; o->anchor_x = leader->fx;
        if (plan != MON_PLAN_ADVANCE)
            o->job = plan == MON_PLAN_HOLD ? MON_JOB_HOLD : MON_JOB_REGROUP;
    }
    if (plan != MON_PLAN_ADVANCE) return;
    /* Packs pursue and surround; they never receive soldier guard assignments. */
    if (!tactical || r->command_grade >= MON_COMMAND_LEADER)
    {
        for (int i = 0; i < count; ++i)
            if (front >= 0 && i != front && !squad_ranged(members[i]))
            {
                members[i]->squad.job = MON_JOB_FLANK;
                members[i]->squad.anchor_y = members[front]->fy;
                members[i]->squad.anchor_x = members[front]->fx;
                break;
            }
    }
    if (!tactical || r->command_grade < MON_COMMAND_LEADER) return;
    monster_type* protect = ranged >= 0 ? members[ranged] : NULL;
    bool withdrawal = false;
    for (int i = 1; i < mon_max; ++i)
    {
        monster_type* ally = &mon_list[i];
        if (!ally->r_idx || ally->hp <= 0 || ally->hp * 2 > ally->maxhp
            || ally->stance != STANCE_FLEEING || !squad_contact(leader, ally)) continue;
        protect = ally; withdrawal = true; break;
    }
    if (!protect) return;
    int guard = -1, best = -100000;
    for (int i = 0; i < count; ++i)
    {
        monster_type* m = members[i];
        const monster_race* mr = &r_info[m->r_idx];
        if (m == protect || squad_ranged(m) || m->hp * 2 <= m->maxhp
            || mr->command_style == MON_COMMAND_PACK) continue;
        int score = mr->pd * mr->ps - 4 * distance(m->fy, m->fx, protect->fy, protect->fx);
        if (score > best) { best = score; guard = i; }
    }
    if (guard >= 0)
    {
        members[guard]->squad.job = withdrawal ? MON_JOB_COVER : MON_JOB_GUARD;
        members[guard]->squad.anchor_y = protect->fy;
        members[guard]->squad.anchor_x = protect->fx;
    }
}

static bool squad_assign_position(monster_type* m, monster_type* leader,
    monster_type* members[SQUAD_MEMBERS], bool assigned[SQUAD_MEMBERS], int count,
    const monster_squad_order* previous)
{
    tactical_context c = { .actor = m, .py = p_ptr->py, .px = p_ptr->px,
        .original_distance = distance(m->fy, m->fx, p_ptr->py, p_ptr->px),
        .ranged = squad_ranged(m), .sight = monster_ai_can_see_player(m),
        .pack = r_info[m->r_idx].command_style == MON_COMMAND_PACK };
    c.oy = m->fy - TACTICAL_RADIUS; c.ox = m->fx - TACTICAL_RADIUS;
    c.shadow = (r_info[m->r_idx].flags2 & RF2_SMART) && r_info[m->r_idx].light < 0;
    for (int i = 0; i < count; ++i)
        if (members[i] != m) c.allies[c.ally_count++] = members[i];
    int costs[TACTICAL_CELLS], steps[TACTICAL_CELLS] = { 0 };
    bool done[TACTICAL_CELLS] = { false };
    for (int i = 0; i < TACTICAL_CELLS; ++i) costs[i] = 100000;
    int start = TACTICAL_RADIUS * TACTICAL_WIDTH + TACTICAL_RADIUS;
    costs[start] = 0;
    int best = -100000, goal = -1;
    for (int visit = 0; visit < TACTICAL_CELLS; ++visit)
    {
        int at = -1;
        for (int i = 0; i < TACTICAL_CELLS; ++i)
            if (!done[i] && costs[i] < 100000 && (at < 0 || costs[i] < costs[at])) at = i;
        if (at < 0) break;
        done[at] = true;
        int y = c.oy + at / TACTICAL_WIDTH, x = c.ox + at % TACTICAL_WIDTH;
        int dist = distance(y, x, c.py, c.px);
        bool reserved = false;
        int formation = 0;
        for (int i = 0; i < count; ++i)
        {
            if (!assigned[i]) continue;
            monster_type* ally = members[i];
            int ay = ally->squad.y, ax = ally->squad.x;
            if (y == ay && x == ax) reserved = true;
            if (squad_ranged(ally) && squad_on_lane(y, x, ay, ax, c.py, c.px))
                formation -= 48;
            if (!c.ranged && dist == 1 && distance(ay, ax, c.py, c.px) == 1
                && (c.pack || r_info[leader->r_idx].command_grade >= MON_COMMAND_LEADER))
            {
                int dot = (y - c.py) * (ay - c.py) + (x - c.px) * (ax - c.px);
                formation += dot < 0 ? 16 : 4;
            }
        }
        /* Ranged units need an actual shot, not just their preferred range.
         * Melee without a reachable contact square gets an approach position. */
        if (!reserved && dist && (m->squad.plan == MON_PLAN_REGROUP || !c.ranged || (dist >= 2
                && projectable(y, x, c.py, c.px, PROJECT_CHCK))))
        {
            int score = tactical_position_score(&c, y, x) + formation - costs[at]
                + squad_job_score(m, y, x);
            if (!c.ranged && dist == 1 && m->squad.plan == MON_PLAN_ADVANCE) score += 32;
            if (c.ranged) score -= ABS(dist - MAX(3, m->best_range)) * 12;
            if (at == start) score += 12; /* Keep an already useful post. */
            if (previous->commander == m->squad.commander
                && previous->target_y == c.py && previous->target_x == c.px
                && previous->y == y && previous->x == x) score += 8;
            if (score > best) { best = score; goal = at; }
        }
        if (steps[at] >= TACTICAL_RADIUS) continue;
        for (int d = 0; d < 8; ++d)
        {
            int yy = y + ddy_ddd[d], xx = x + ddx_ddd[d];
            int ry = yy - c.oy, rx = xx - c.ox;
            if (ry < 0 || ry >= TACTICAL_WIDTH || rx < 0 || rx >= TACTICAL_WIDTH
                || !squad_enterable(m, leader, yy, xx)) continue;
            int next = ry * TACTICAL_WIDTH + rx;
            int cost = costs[at] + monster_step_cost(m, y, x, yy, xx) * 4
                + tactical_reaction_cost(&c, y, x, yy, xx)
                + monster_terrain_penalty(m, yy, xx) * 8;
            if (cost < costs[next])
            { costs[next] = cost; steps[next] = steps[at] + 1; }
        }
    }
    if (goal < 0) return false;
    m->squad.y = c.oy + goal / TACTICAL_WIDTH;
    m->squad.x = c.ox + goal % TACTICAL_WIDTH;
    monster_senses_share_trace(m, c.py, c.px);
    return true;
}

void monster_squad_prepare(void)
{
    monster_squad_order previous[MAX_MONSTERS];
    bool considered[MAX_MONSTERS] = { false };
    int followers[MAX_MONSTERS] = { 0 };
    for (int i = 1; i < mon_max; ++i)
    {
        previous[i] = mon_list[i].squad;
        memset(&mon_list[i].squad, 0, sizeof(mon_list[i].squad));
    }
    for (int i = 1; i < mon_max; ++i)
    {
        int leader = previous[i].commander;
        if (leader > 0 && leader < mon_max && leader != i
            && previous[leader].role == MON_SQUAD_COMMANDER
            && squad_available(&mon_list[i]) && squad_available(&mon_list[leader])
            && squad_contact(&mon_list[leader], &mon_list[i])) ++followers[leader];
    }
    for (int squad = 0; squad < SQUAD_MAX;)
    {
        int leader_idx = 0, rank = 0;
        for (int i = 1; i < mon_max; ++i)
        {
            monster_type* m = &mon_list[i];
            if (considered[i] || m->squad.role || !squad_available(m)) continue;
            int candidate = squad_command_rank(m);
            if (candidate && followers[i]) candidate += 512;
            if (candidate > rank && monster_ai_can_see_player(m))
            { leader_idx = i; rank = candidate; }
        }
        if (!leader_idx) break;
        considered[leader_idx] = true;
        monster_type* leader = &mon_list[leader_idx];
        monster_type* members[SQUAD_MEMBERS] = { leader };
        bool assigned[SQUAD_MEMBERS] = { false };
        int count = 1;
        /* Retain viable members before recruiting new nearby allies. */
        while (count < SQUAD_MEMBERS)
        {
            monster_type* next = NULL;
            int closest = 100000;
            for (int i = 1; i < mon_max; ++i)
            {
                monster_type* m = &mon_list[i];
                if (m->squad.role || !squad_available(m) || !squad_contact(leader, m)) continue;
                bool member = false;
                for (int n = 0; n < count; ++n) if (members[n] == m) member = true;
                if (member) continue;
                int dist = distance(m->fy, m->fx, p_ptr->py, p_ptr->px);
                if (previous[i].commander != leader_idx) dist += 64;
                if (dist < closest) { closest = dist; next = m; }
            }
            if (!next) break;
            members[count++] = next;
        }
        if (count < 2) continue;
        ++squad;
        for (int i = 0; i < count; ++i)
        {
            monster_type* m = members[i];
            m->squad = (monster_squad_order){ .commander = leader_idx,
                .role = i ? MON_SQUAD_SOLDIER : MON_SQUAD_COMMANDER,
                .y = m->fy, .x = m->fx, .target_y = p_ptr->py, .target_x = p_ptr->px };
        }
        squad_assign_jobs(leader, members, count, &previous[leader_idx]);
        /* Reserve useful firing positions first, then arrange the melee line. */
        for (int ranged = 1; ranged >= 0; --ranged)
            for (int i = 0; i < count; ++i)
            {
                monster_type* m = members[i];
                if (squad_ranged(m) != (bool)ranged) continue;
                int idx = (int)(m - mon_list);
                assigned[i] = squad_assign_position(m, leader, members, assigned,
                    count, &previous[idx]);
                if (!assigned[i]) m->squad.job = MON_JOB_NONE;
            }
    }
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
    c.commanded = monster_squad_order_valid(m_ptr);
    c.pack = r_ptr->command_style == MON_COMMAND_PACK;
    c.actor = m_ptr;
    if (c.sight) { c.py = p_ptr->py; c.px = p_ptr->px; }
    else if (c.commanded) { c.py = m_ptr->squad.target_y; c.px = m_ptr->squad.target_x; }
    else if (!c.shadow || !monster_senses_target(m_ptr, &c.py, &c.px)) return false;
    c.oy = m_ptr->fy - TACTICAL_RADIUS; c.ox = m_ptr->fx - TACTICAL_RADIUS;
    c.original_distance = distance(m_ptr->fy, m_ptr->fx, c.py, c.px);
    if (c.original_distance > MAX(6, m_ptr->best_range + 1)) return false;
    c.ranged = m_ptr->min_range > 1 || (r_ptr->flags1 & RF1_NEVER_BLOW)
        || (c.commanded && squad_ranged(m_ptr));
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
                || (c.commanded && m_ptr->squad.plan == MON_PLAN_REGROUP)
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
    /* Explain only an improvement already made by the first step, not an
     * unexecuted second step of the local plan. Previews remain side-effect free. */
    int before[MON_TACTIC_MAX], after[MON_TACTIC_MAX], improvement = 3;
    tactical_position_details(&c, m_ptr->fy, m_ptr->fx, before);
    tactical_position_details(&c, choice->y, choice->x, after);
    choice->reason = MON_TACTIC_NONE;
    for (int i = 1; i < MON_TACTIC_MAX; ++i)
    {
        /* Message details may use player visibility; the decision above may
         * not. Do not reveal an unseen archer or wounded ally through prose. */
        if (c.pack && (i == MON_TACTIC_GUARD || i == MON_TACTIC_WITHDRAWAL)) continue;
        if (i == MON_TACTIC_GUARD || i == MON_TACTIC_WITHDRAWAL)
        {
            bool withdrawal = i == MON_TACTIC_WITHDRAWAL;
            before[i] = tactical_guard_score_visible(&c, m_ptr->fy, m_ptr->fx,
                withdrawal, true);
            after[i] = tactical_guard_score_visible(&c, choice->y, choice->x,
                withdrawal, true);
        }
        if (i == MON_TACTIC_ORDER && (!c.commanded
                || m_ptr->squad.role != MON_SQUAD_SOLDIER
                || !mon_list[m_ptr->squad.commander].ml)) continue;
        if (i == MON_TACTIC_IMPALE || i == MON_TACTIC_SWEEP)
        {
            bool visible_ally = false;
            for (int n = 0; n < c.ally_count; ++n)
            {
                monster_type* ally = c.allies[n];
                if (ally->ml && (i == MON_TACTIC_IMPALE
                        ? tactical_impale_pair(c.py, c.px, m_ptr->fy, m_ptr->fx,
                            ally->fy, ally->fx)
                        : distance(ally->fy, ally->fx, c.py, c.px) == 1))
                { visible_ally = true; break; }
            }
            if (!visible_ally) continue;
        }
        if (after[i] - before[i] > improvement)
        { improvement = after[i] - before[i]; choice->reason = i; }
    }
    if (choice->reason == MON_TACTIC_ORDER && c.pack) choice->reason = MON_TACTIC_PACK;
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
    monster_ai_plan_feedback(m_ptr, choice.reason, choice.y, choice.x);
    /* Only execution commits. Reusing an anchor cannot refresh its budget. */
    if (!m_ptr->ai.goal_age || m_ptr->ai.goal_y != choice.goal_y || m_ptr->ai.goal_x != choice.goal_x)
    {
        m_ptr->ai.goal_y = choice.goal_y; m_ptr->ai.goal_x = choice.goal_x;
        m_ptr->ai.goal_age = 3;
    }
    return true;
}

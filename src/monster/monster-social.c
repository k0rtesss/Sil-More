#include "angband.h"
#include "externs.h"
#include "monster/monster-social.h"
#include "monster/monster-senses.h"
#include "cave/cave-environment.h"
#include "cave/cave-events.h"
#include "melee/melee-movement.h"

#define SOCIAL_RADIUS 8
#define WITNESS_RADIUS 6
#define QUARREL_ONE_IN 500

/* Level-local diplomacy, independent of the player's allegiance and truce. */
static byte relations[MON_GROUP_MAX][MON_GROUP_MAX];

static bool live_index(int i)
{
    return i > 0 && i < mon_max && mon_list[i].r_idx;
}

static bool participant(const monster_type* m)
{
    if (!m || !m->r_idx) return false;
    const monster_race* r = &r_info[m->r_idx];
    /* Quest givers and miners retain their dedicated behaviours. */
    return !(r->flags1 & (RF1_UNIQUE | RF1_PEACEFUL))
        && m->r_idx != R_IDX_HUMAN_THRALL && m->r_idx != R_IDX_ELF_THRALL
        && m->r_idx != R_IDX_ALERT_HUMAN_THRALL
        && m->r_idx != R_IDX_ALERT_ELF_THRALL;
}

int monster_social_group(const monster_type* m)
{
    if (!m || !m->r_idx) return MON_GROUP_OTHER;
    if (m->social_group > MON_GROUP_AUTO && m->social_group < MON_GROUP_MAX)
        return m->social_group;
    if (m->r_idx == R_IDX_HUMAN_THRALL || m->r_idx == R_IDX_ELF_THRALL
        || m->r_idx == R_IDX_ALERT_HUMAN_THRALL
        || m->r_idx == R_IDX_ALERT_ELF_THRALL) return MON_GROUP_THRALL;
    u32b flags = r_info[m->r_idx].flags3;
    if (flags & RF3_ORC) return MON_GROUP_ORC;
    if (flags & RF3_MAN) return MON_GROUP_MAN;
    if (flags & RF3_ELF) return MON_GROUP_ELF;
    if (flags & RF3_TROLL) return MON_GROUP_TROLL;
    if (flags & RF3_RAUKO) return MON_GROUP_RAUKO;
    return MON_GROUP_OTHER;
}

static void forget_rival(monster_type* m)
{
    m->social_rival = 0;
    m->social_memory = 0;
    m->social_cooldown = MON_SOCIAL_COOLDOWN;
    m->social_state = MON_SOCIAL_NONE;
    m->social_timer = 0;
    m->social_focus = m->social_ally = 0;
}

static int social_index(const monster_type* m)
{
    int idx = in_bounds_fully(m->fy, m->fx) ? cave_m_idx[m->fy][m->fx] : 0;
    return live_index(idx) && &mon_list[idx] == m ? idx : 0;
}

/* Ending a quarrel also releases its helpers. It never changes diplomacy. */
static void end_dispute(monster_type* m)
{
    int own = social_index(m), rival = m->social_rival;
    if (live_index(rival) && mon_list[rival].social_rival == own)
        forget_rival(&mon_list[rival]);
    forget_rival(m);
    for (int i = 1; i < mon_max; i++)
        if (mon_list[i].r_idx && mon_list[i].social_state == MON_SOCIAL_HELP
            && (mon_list[i].social_ally == own || mon_list[i].social_ally == rival))
            forget_rival(&mon_list[i]);
}

/* Called before deletion or list compaction. Never let a reused slot inherit
 * someone else's feud. Membership itself moves with the monster record. */
void monster_social_remap(int old_idx, int new_idx)
{
    for (int i = 1; i < mon_max; i++)
    {
        monster_type* m = &mon_list[i];
        if (!m->r_idx) continue;
        if (!new_idx && (m->social_rival == old_idx
                || m->social_focus == old_idx || m->social_ally == old_idx))
            forget_rival(m);
        else if (new_idx)
        {
            if (m->social_rival == old_idx) m->social_rival = new_idx;
            if (m->social_focus == old_idx) m->social_focus = new_idx;
            if (m->social_ally == old_idx) m->social_ally = new_idx;
        }
    }
}

bool monster_social_set_group(monster_type* m, int group)
{
    if (!m || !m->r_idx || group < MON_GROUP_AUTO || group >= MON_GROUP_MAX)
        return false;
    /* Changing sides ends personal feuds involving this creature. */
    bool engaged = m->social_state != MON_SOCIAL_NONE;
    int idx = social_index(m);
    if (idx) monster_social_remap(idx, 0);
    forget_rival(m);
    if (!engaged) m->social_cooldown = 0;
    m->social_group = group;
    return true;
}

int monster_group_relation(int a, int b)
{
    if (a <= MON_GROUP_AUTO || a >= MON_GROUP_MAX
        || b <= MON_GROUP_AUTO || b >= MON_GROUP_MAX) return MON_REL_NEUTRAL;
    return relations[a][b];
}

bool monster_group_set_relation(int a, int b, int relation)
{
    if (a <= MON_GROUP_AUTO || a >= MON_GROUP_MAX
        || b <= MON_GROUP_AUTO || b >= MON_GROUP_MAX
        || (relation != MON_REL_NEUTRAL && relation != MON_REL_HOSTILE)
        || (a == b && relation != MON_REL_NEUTRAL)) return false;
    relations[a][b] = relations[b][a] = relation;
    if (relation == MON_REL_NEUTRAL)
        for (int i = 1; i < mon_max; i++)
        {
            monster_type* m = &mon_list[i];
            if (!m->r_idx || !live_index(m->social_rival)) continue;
            int from = monster_social_group(m);
            int to = monster_social_group(&mon_list[m->social_rival]);
            if ((from == a && to == b) || (from == b && to == a))
                end_dispute(m);
        }
    return true;
}

int monster_social_relation(const monster_type* a, const monster_type* b)
{
    if (!a || !b || a == b || !a->r_idx || !b->r_idx) return MON_REL_NEUTRAL;
    if ((live_index(a->social_rival) && &mon_list[a->social_rival] == b
            && a->social_memory)
        || (live_index(b->social_rival) && &mon_list[b->social_rival] == a
            && b->social_memory)) return MON_REL_HOSTILE;
    if ((a->social_state == MON_SOCIAL_HELP && live_index(a->social_focus)
            && &mon_list[a->social_focus] == b)
        || (b->social_state == MON_SOCIAL_HELP && live_index(b->social_focus)
            && &mon_list[b->social_focus] == a)) return MON_REL_HOSTILE;
    return monster_group_relation(monster_social_group(a), monster_social_group(b));
}

bool monster_social_allies(const monster_type* a, const monster_type* b)
{
    if (!a || !b || !a->r_idx || !b->r_idx
        || monster_social_relation(a, b) == MON_REL_HOSTILE) return false;
    const monster_race* ar = &r_info[a->r_idx];
    const monster_race* br = &r_info[b->r_idx];
    if ((ar->flags1 | br->flags1) & RF1_PEACEFUL) return false;
    int ag = monster_social_group(a), bg = monster_social_group(b);
    /* Explicit membership overrides old race/glyph and wandering associations. */
    if (a->social_group || b->social_group) return ag == bg;
    if (ag == MON_GROUP_THRALL || bg == MON_GROUP_THRALL) return ag == bg;
    if (a->wandering_idx >= FLOW_WANDERING_HEAD
        && a->wandering_idx <= FLOW_WANDERING_TAIL
        && a->wandering_idx == b->wandering_idx) return true;
    if (ag != MON_GROUP_OTHER || bg != MON_GROUP_OTHER) return ag == bg;
    /* OTHER is a fallback, not a faction containing every animal and horror. */
    return ar->d_char == br->d_char
        || ((ar->flags3 & br->flags3)
            & (RF3_DRAGON | RF3_SERPENT | RF3_HORROR | RF3_WOLF | RF3_CAT));
}

bool monster_social_start_feud(int a, int b)
{
    if (!live_index(a) || !live_index(b) || a == b
        || !participant(&mon_list[a]) || !participant(&mon_list[b])) return false;
    monster_type* m = &mon_list[a];
    monster_type* n = &mon_list[b];
    /* A local quarrel has two participants; do not steal an existing rival. */
    if (m->social_state != MON_SOCIAL_NONE || n->social_state != MON_SOCIAL_NONE)
        return m->social_rival == b && n->social_rival == a;
    m->social_rival = b; n->social_rival = a;
    m->social_memory = n->social_memory = MON_SOCIAL_MEMORY;
    m->social_state = n->social_state = MON_SOCIAL_CHALLENGE;
    m->social_timer = n->social_timer = 2;
    return true;
}

void monster_social_player_attack(monster_type* m)
{
    if (m && m->r_idx) m->social_player_threat = 3;
}

void monster_social_reset(void)
{
    memset(relations, 0, sizeof(relations));
    for (int i = 1; i < mon_max; i++)
    {
        mon_list[i].social_group = MON_GROUP_AUTO;
        mon_list[i].social_rival = 0;
        mon_list[i].social_memory = mon_list[i].social_cooldown = 0;
        mon_list[i].social_state = mon_list[i].social_timer = 0;
        mon_list[i].social_focus = mon_list[i].social_ally = 0;
        mon_list[i].social_player_threat = 0;
    }
}

bool monster_social_valid(const monster_type* m)
{
    if (m->social_group >= MON_GROUP_MAX || m->social_memory > MON_SOCIAL_MEMORY
        || m->social_cooldown > MON_SOCIAL_COOLDOWN || m->social_player_threat > 3)
        return false;
    if (m->social_state == MON_SOCIAL_CHALLENGE || m->social_state == MON_SOCIAL_FIGHT)
        return live_index(m->social_rival) && &mon_list[m->social_rival] != m
            && m->social_memory && !m->social_focus && !m->social_ally
            && m->social_timer && m->social_timer <= (m->social_state == MON_SOCIAL_CHALLENGE
                ? 2 : MON_SOCIAL_DISPUTE_ACTIONS);
    if (m->social_rival || m->social_memory) return false;
    if (m->social_state == MON_SOCIAL_NONE)
        return !m->social_timer && !m->social_focus && !m->social_ally;
    if ((m->social_state != MON_SOCIAL_HELP && m->social_state != MON_SOCIAL_AVOID)
        || !live_index(m->social_focus) || &mon_list[m->social_focus] == m
        || !m->social_timer || m->social_timer > MON_SOCIAL_RESPONSE_ACTIONS)
        return false;
    return m->social_state == MON_SOCIAL_AVOID ? !m->social_ally
        : live_index(m->social_ally) && &mon_list[m->social_ally] != m
            && m->social_ally != m->social_focus;
}

static bool sees(const monster_type* m, const monster_type* n)
{
    int radius = (r_info[m->r_idx].flags2 & RF2_SHORT_SIGHTED) ? 2 : SOCIAL_RADIUS;
    return distance(m->fy, m->fx, n->fy, n->fx) <= radius
        && los(m->fy, m->fx, n->fy, n->fx);
}

void monster_social_tick(monster_type* m)
{
    if (m->social_cooldown) --m->social_cooldown;
    if (m->social_player_threat) --m->social_player_threat;
    if (m->social_state == MON_SOCIAL_HELP
        && (!live_index(m->social_ally) || !live_index(m->social_focus)
            || mon_list[m->social_ally].social_rival != m->social_focus))
        forget_rival(m);
    if (!m->social_rival) return;
    if (!live_index(m->social_rival)) { forget_rival(m); return; }
    monster_type* n = &mon_list[m->social_rival];
    if (m->alertness >= ALERTNESS_UNWARY && sees(m, n))
        m->social_memory = MON_SOCIAL_MEMORY;
    else if (!m->social_memory || !--m->social_memory)
    {
        end_dispute(m);
    }
}

static bool available(monster_type* m)
{
    return participant(m) && m->alertness >= ALERTNESS_UNWARY
        && !m->confused && !m->stunned && !m->skip_this_turn && !m->skip_next_turn
        && !m->smite_recovery;
}

static int physical_blow(const monster_type* m)
{
    const monster_race* r = &r_info[m->r_idx];
    if (r->flags1 & RF1_NEVER_BLOW) return -1;
    for (int b = 0; b < MONSTER_BLOW_MAX; b++)
        if (r->blow[b].method && r->blow[b].dd && r->blow[b].ds)
            return b;
    return -1;
}

static void attack(monster_type* m, int victim, int b)
{
    monster_type* n = &mon_list[victim];
    const monster_race* r = &r_info[n->r_idx];
    const monster_blow* blow = &r_info[m->r_idx].blow[b];
    char attacker[80], defender[80];
    bool visible = m->ml && n->ml;
    m->visual_facing_dir = rough_direction(m->fy, m->fx, n->fy, n->fx);
    m->previous_action[0] = ACTION_MISC;
    monster_sound(m, MONSTER_SOUND_MELEE_BASE + b);
    /* Gameplay noise is independent of audio settings. Both hits and misses
     * create a local racket: walls/doors attenuate it and it fades with time. */
    cave_event_emit(CAVE_EVENT_FIGHT, m->fy, m->fx, 24);
    m->noise = MAX(m->noise, 10);
    /* Player combat helpers include player bane, position and curse bonuses.
     * Use an opposed d20 and the creatures' own physical stats here. Spell
     * effects and player-specific melee abilities are deliberately excluded. */
    int evasion = r->evn - n->song_evasion_penalty - (n->stunned ? 2 : 0);
    if (n->alertness < ALERTNESS_UNWARY) evasion = -5;
    bool hit = dieroll(20) + blow->att > dieroll(20) + evasion;
    int damage = 0;
    if (hit)
    {
        int dd = MAX(1, blow->dd - m->blow_dd_reduction[b]);
        int ds = MAX(1, blow->ds - m->blow_ds_reduction[b]);
        int pd = MAX(0, r->pd - n->song_armor_dice_penalty);
        int ps = monster_base_armour_sides(n);
        damage = MAX(0, damroll(dd, ds) - damroll(pd, ps));
    }
    if (visible)
    {
        monster_desc(attacker, sizeof(attacker), m, 0);
        monster_desc(defender, sizeof(defender), n, 0);
        msg_format(hit ? "%^s strikes %s." : "%^s misses %s.", attacker, defender);
    }
    /* Even a miss wakes a sleeper. ALERTNESS_ALERT means noticing the player
     * and queues a skipped turn, so personal hostility stays separate. */
    set_alertness(n, MAX(n->alertness, ALERTNESS_UNWARY));
    n->hp -= damage;
    if (damage && n->hp > 0) monster_sound(n, MONSTER_SOUND_DAMAGE);
    if (p_ptr->health_who == victim) p_ptr->redraw |= PR_HEALTHBAR;
    p_ptr->window |= PW_MONLIST | PW_MONSTER;
    lite_spot(n->fy, n->fx);
    if (n->hp > 0) return;
    if (n->ml)
    {
        monster_desc(defender, sizeof(defender), n, 0);
        msg_format("%^s is slain.", defender);
    }
    /* Ambient kills drop possessions/loot, but never grant player experience,
     * kill statistics, quest progress, or mercy-quest failures. */
    monster_sound(n, MONSTER_SOUND_DEATH);
    if (!(r->flags2 & RF2_TERRITORIAL))
        drop_loot(n);
    else
    {
        /* Territorial races already placed their generated treasure at spawn.
         * Only release possessions acquired since then. */
        while (n->hold_o_idx)
        {
            int idx = n->hold_o_idx;
            object_type loot;
            object_copy(&loot, &o_list[idx]);
            n->hold_o_idx = o_list[idx].next_o_idx;
            o_list[idx].held_m_idx = 0;
            delete_object_idx(idx);
            loot.held_m_idx = loot.next_o_idx = 0;
            loot.ident &= ~IDENT_HIDE_CARRY;
            drop_near(&loot, -1, n->fy, n->fx);
        }
    }
    delete_monster_idx(victim);
}

static bool safe_step(monster_type* m, int y, int x)
{
    if (!in_bounds_fully(y, x) || cave_m_idx[y][x] || cave_trap_bold(y, x)
        || !cave_exist_mon(&r_info[m->r_idx], y, x, false, false)) return false;
    int pending = cave_environment_pending_hazard(y, x);
    return !pending && cave_feat[y][x] != FEAT_DEEP_WATER
        && cave_feat[y][x] != FEAT_POISON && cave_feat[y][x] != FEAT_LAVA
        && cave_feat[y][x] != FEAT_CHASM;
}

/* A distant sighting or remembered trail is less urgent than a nearby rival.
 * Actual player aggression and a nearby visible player still take priority. */
static bool player_urgent(const monster_type* m, int rival_distance)
{
    return m->social_player_threat || (monster_has_sight(m)
        && distance(m->fy, m->fx, p_ptr->py, p_ptr->px) <= MAX(3, rival_distance));
}

static bool outmatched(const monster_type* m, const monster_type* rival)
{
    const monster_race* r = &r_info[m->r_idx];
    if (r->flags2 & RF2_MINDLESS) return false;
    /* Self-preservation is independent of alertness toward the player. */
    if (m->hp * 2 <= m->maxhp || m->stance == STANCE_FLEEING) return true;
    return !(r->flags3 & RF3_NO_FEAR)
        && r_info[rival->r_idx].level >= r->level + 4;
}

static void social_message(monster_type* m, const monster_type* n, const char* verb)
{
    if (m->ml && n->ml)
    {
        char a[80], b[80];
        monster_desc(a, sizeof(a), m, 0);
        monster_desc(b, sizeof(b), n, 0);
        msg_format("%^s %s %s.", a, verb, b);
    }
    p_ptr->window |= PW_MONLIST | PW_MONSTER;
}

static void withdraw(monster_type* m, int threat, bool yield)
{
    if (yield) end_dispute(m);
    else forget_rival(m);
    m->social_state = MON_SOCIAL_AVOID;
    m->social_focus = threat;
    m->social_timer = MON_SOCIAL_RESPONSE_ACTIONS;
    social_message(m, &mon_list[threat], yield ? "yields to" : "backs away from");
}

/* Paid movement away from the observed threat, without a player-centred flow.
 * Prefer separation from every visible hostile, not just the focus creature. */
static bool retreat(monster_type* m)
{
    if (!live_index(m->social_focus)) { forget_rival(m); return false; }
    monster_type* n = &mon_list[m->social_focus];
    int dist = distance(m->fy, m->fx, n->fy, n->fx);
    if (!sees(m, n) || dist >= 4 || !m->social_timer)
    { forget_rival(m); return true; }
    --m->social_timer;
    int best = -100000, by = m->fy, bx = m->fx;
    if (!(r_info[m->r_idx].flags1 & RF1_NEVER_MOVE))
        for (int k = 0; k < 8; k++)
        {
            int y = m->fy + ddy_ddd[k], x = m->fx + ddx_ddd[k];
            int d = distance(y, x, n->fy, n->fx);
            if (d < dist || !safe_step(m, y, x)) continue;
            int score = d * 20;
            for (int i = 1; i < mon_max; i++)
                if (mon_list[i].r_idx && &mon_list[i] != m && sees(m, &mon_list[i])
                    && monster_social_relation(m, &mon_list[i]) == MON_REL_HOSTILE)
                    score -= MAX(0, 4 - distance(y, x, mon_list[i].fy, mon_list[i].fx)) * 30;
            if (monster_has_sight(m))
                score -= MAX(0, 4 - distance(y, x, p_ptr->py, p_ptr->px)) * 30;
            if (score > best) { best = score; by = y; bx = x; }
        }
    if (by != m->fy || bx != m->fx) process_move(m, by, bx, false);
    if (!m->social_timer) forget_rival(m);
    /* A cornered loser holds/yields; it does not resume attacking this action. */
    return true;
}

static int helpers_for(int ally)
{
    int count = 0;
    for (int i = 1; i < mon_max; i++)
        if (mon_list[i].r_idx && mon_list[i].social_state == MON_SOCIAL_HELP
            && mon_list[i].social_ally == ally) ++count;
    return count;
}

/* Witnesses only take sides in a directly observed dispute. A noise may bring
 * an investigator here, but never identifies the participants through walls. */
static bool witness(monster_type* m)
{
    const monster_race* r = &r_info[m->r_idx];
    if (m->social_state || m->social_cooldown || (r->flags2 & RF2_MINDLESS)) return false;
    int first = 0, second = 0, nearest = WITNESS_RADIUS + 1;
    /* Bound routine observation work independently of the level population. */
    for (int y = MAX(1, m->fy - WITNESS_RADIUS);
         y <= MIN(p_ptr->cur_map_hgt - 2, m->fy + WITNESS_RADIUS); y++)
    for (int x = MAX(1, m->fx - WITNESS_RADIUS);
         x <= MIN(p_ptr->cur_map_wid - 2, m->fx + WITNESS_RADIUS); x++)
    {
        int i = cave_m_idx[y][x];
        if (!live_index(i)) continue;
        monster_type* a = &mon_list[i];
        int j = a->social_rival;
        if (!a->r_idx || j <= i || !live_index(j) || &mon_list[j] == m || a == m
            || (a->social_state != MON_SOCIAL_FIGHT && a->social_state != MON_SOCIAL_CHALLENGE)
            || mon_list[j].social_rival != i || !sees(m, a) || !sees(m, &mon_list[j])) continue;
        int d = MAX(distance(m->fy, m->fx, a->fy, a->fx),
            distance(m->fy, m->fx, mon_list[j].fy, mon_list[j].fx));
        if (d < nearest || (d == nearest && first && i < first))
        { first = i; second = j; nearest = d; }
    }
    if (!first || player_urgent(m, nearest)) return false;
    monster_type* a = &mon_list[first];
    monster_type* b = &mon_list[second];
    bool ally_a = monster_social_allies(m, a), ally_b = monster_social_allies(m, b);
    if (ally_a && ally_b && (r->flags1 & (RF1_ESCORT | RF1_ESCORTS))
        && r->level >= MAX(r_info[a->r_idx].level, r_info[b->r_idx].level)
        && m->hp * 2 > m->maxhp && m->stance != STANCE_FLEEING)
    {
        social_message(m, a, "orders an end to the quarrel involving");
        end_dispute(a);
        m->social_cooldown = MON_SOCIAL_COOLDOWN;
        return true;
    }
    /* Challenges are an opportunity to back down, not an invitation to mob. */
    if (a->social_state != MON_SOCIAL_FIGHT) return false;
    if (ally_a != ally_b)
    {
        int ally = ally_a ? first : second, enemy = ally_a ? second : first;
        if (!outmatched(m, &mon_list[enemy]) && physical_blow(m) >= 0
            && helpers_for(ally) < 2)
        {
            m->social_state = MON_SOCIAL_HELP;
            m->social_focus = enemy; m->social_ally = ally;
            m->social_timer = MON_SOCIAL_RESPONSE_ACTIONS;
            social_message(m, &mon_list[ally], "moves to defend");
            return true;
        }
        if (outmatched(m, &mon_list[enemy])) { withdraw(m, enemy, false); return retreat(m); }
    }
    else if (!(r->flags3 & RF3_NO_FEAR)
        && (outmatched(m, a) || outmatched(m, b)))
    {
        int enemy = distance(m->fy, m->fx, a->fy, a->fx)
            <= distance(m->fy, m->fx, b->fy, b->fx) ? first : second;
        withdraw(m, enemy, false);
        return retreat(m);
    }
    return false;
}

static bool approach(monster_type* m, monster_type* n)
{
    if ((r_info[m->r_idx].flags1 & RF1_NEVER_MOVE)
        || (r_info[m->r_idx].flags2 & RF2_TERRITORIAL)) return false;
    int dist = distance(m->fy, m->fx, n->fy, n->fx);
    for (int k = 0; k < 8; k++)
    {
        int y = m->fy + ddy_ddd[k], x = m->fx + ddx_ddd[k];
        if (distance(y, x, n->fy, n->fx) < dist && safe_step(m, y, x))
        {
            process_move(m, y, x, false);
            return true;
        }
    }
    return false;
}

bool monster_social_turn(monster_type* m)
{
    if (!available(m) || p_ptr->truce
        || cave_environment_pending_hazard(m->fy, m->fx)) return false;
    if (m->social_state == MON_SOCIAL_AVOID)
    {
        if (player_urgent(m, 0)) return false;
        return retreat(m);
    }
    if (m->social_state == MON_SOCIAL_HELP
        && (!live_index(m->social_ally) || !live_index(m->social_focus)
            || mon_list[m->social_ally].social_rival != m->social_focus
            || !sees(m, &mon_list[m->social_focus]) || !m->social_timer))
        forget_rival(m);

    int victim = 0, nearest = SOCIAL_RADIUS + 1;
    bool has_enemies = m->social_rival || m->social_state == MON_SOCIAL_HELP;
    int group = monster_social_group(m);
    for (int g = 1; !has_enemies && g < MON_GROUP_MAX; g++)
        has_enemies = relations[group][g] == MON_REL_HOSTILE;
    for (int i = 1; has_enemies && i < mon_max; i++)
    {
        monster_type* n = &mon_list[i];
        if (n == m || !participant(n)
            || monster_social_relation(m, n) != MON_REL_HOSTILE) continue;
        int d = distance(m->fy, m->fx, n->fy, n->fx);
        if (d < nearest && sees(m, n)) { nearest = d; victim = i; }
    }
    if (victim)
    {
        monster_type* n = &mon_list[victim];
        if (player_urgent(m, nearest)) return false;
        if (outmatched(m, n))
        {
            /* Once safely separated, don't repeatedly announce withdrawal
             * from a distant faction enemy while on a disengagement cooldown. */
            if (!m->social_state && m->social_cooldown && nearest >= 4) return false;
            withdraw(m, victim, m->social_rival != 0);
            return retreat(m);
        }
        /* An actual clash between hostile groups also supplies witnesses with
         * identified combatants. It has no friendly challenge phase, and ending
         * this local clash never clears the groups' underlying hostility. */
        if (!m->social_state && !n->social_state
            && monster_group_relation(group, monster_social_group(n)) == MON_REL_HOSTILE
            && monster_social_start_feud(social_index(m), victim))
        {
            m->social_state = n->social_state = MON_SOCIAL_FIGHT;
            m->social_timer = n->social_timer = MON_SOCIAL_DISPUTE_ACTIONS;
        }
        if (m->social_state == MON_SOCIAL_CHALLENGE)
        {
            if (--m->social_timer == 0)
            {
                monster_type* rival = &mon_list[m->social_rival];
                m->social_state = rival->social_state = MON_SOCIAL_FIGHT;
                m->social_timer = rival->social_timer = MON_SOCIAL_DISPUTE_ACTIONS;
                social_message(m, rival, "accepts the challenge from");
            }
            else social_message(m, n, "stands up to");
            return true;
        }
        if (m->social_state == MON_SOCIAL_FIGHT && !--m->social_timer)
        {
            withdraw(m, victim, true);
            return retreat(m);
        }
        if (m->social_state == MON_SOCIAL_HELP) --m->social_timer;
        int blow = physical_blow(m);
        bool acted;
        if (nearest == 1 && blow >= 0) { attack(m, victim, blow); acted = true; }
        else acted = approach(m, n);
        if (m->social_state == MON_SOCIAL_HELP && !m->social_timer) forget_rival(m);
        return acted;
    }
    if (witness(m)) return true;
    int y, x;
    if (m->social_state || m->social_cooldown || m->stance == STANCE_FLEEING
        || m->social_player_threat || monster_senses_target(m, &y, &x)
        || physical_blow(m) < 0 || !(r_info[m->r_idx].flags3 & RF3_ORC)
        || (r_info[m->r_idx].flags2 & RF2_MINDLESS)) return false;
    for (int k = 0; k < 8; k++)
    {
        y = m->fy + ddy_ddd[k]; x = m->fx + ddx_ddd[k];
        if (!in_bounds_fully(y, x)) continue;
        int idx = cave_m_idx[y][x];
        if (!live_index(idx)) continue;
        monster_type* n = &mon_list[idx];
        if (!participant(n) || n->social_state || n->social_cooldown
            || !(r_info[n->r_idx].flags3 & (RF3_ORC | RF3_MAN | RF3_ELF))) continue;
        monster_senses_refresh(n);
        int ty, tx;
        if (!available(n) || n->stance == STANCE_FLEEING || n->social_player_threat
            || physical_blow(n) < 0 || !sees(m, n)
            || monster_senses_target(n, &ty, &tx)) continue;
        if (!one_in_(QUARREL_ONE_IN)) break;
        if (!monster_social_start_feud(social_index(m), idx)) break;
        cave_event_emit(CAVE_EVENT_FIGHT, m->fy, m->fx, 12);
        social_message(m, n, "challenges");
        return true;
    }
    return false;
}

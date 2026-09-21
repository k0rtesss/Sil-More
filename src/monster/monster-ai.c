#include "angband.h"
#include "externs.h"
#include "monster/monster-ai.h"
#include "monster/monster-senses.h"
#include "monster/monster-social.h"
#include <string.h>

_Static_assert(MON_AI_SLAY_FEAR + 1 == MON_AI_FEATURE_COUNT,
    "Monster observation save record must cover every feature");

static int observation_lifetime(int feature)
{
    if (feature == MON_AI_KITING || feature == MON_AI_STEALTH
        || feature == MON_AI_WOUNDED || feature == MON_AI_POISON_PRESSURE)
        return 8;
    if ((feature >= MON_AI_FLANKING && feature <= MON_AI_SONG)
        || feature >= MON_AI_IMPALE)
        return 20;
    return 40;
}

bool monster_ai_enabled(const monster_type* m_ptr)
{
    return m_ptr && m_ptr->r_idx > 0 && m_ptr->r_idx != R_IDX_MORGOTH
        && !(r_info[m_ptr->r_idx].flags2 & RF2_MINDLESS);
}

bool monster_ai_can_see_player(const monster_type* m_ptr)
{
    return m_ptr && m_ptr->r_idx > 0 && monster_has_sight(m_ptr);
}

static int observation_remaining(const monster_ai_observation* observation)
{
    s32b age = playerturn - observation->turn;
    if (age < 0 || age >= observation->ttl)
        return 0;
    return observation->ttl - age;
}

int monster_ai_confidence(const monster_type* m_ptr, int feature)
{
    if (!monster_ai_enabled(m_ptr) || feature < 0
        || feature >= MON_AI_FEATURE_COUNT)
        return 0;
    const monster_ai_observation* observation = &m_ptr->ai.observations[feature];
    if (!observation_remaining(observation))
        return 0;
    return observation->value;
}

void monster_ai_observe(monster_type* m_ptr, int feature, int evidence)
{
    if (!monster_ai_enabled(m_ptr) || feature < 0
        || feature >= MON_AI_FEATURE_COUNT || !evidence || m_ptr->confused
        || m_ptr->alertness < ALERTNESS_ALERT
        || !monster_ai_can_see_player(m_ptr))
        return;
    monster_ai_observation* observation = &m_ptr->ai.observations[feature];
    /* Several blows or several witnesses in one action are not independent
     * evidence. An absent record at playerturn zero is still writable. */
    if (observation->ttl && observation->turn == playerturn)
        return;
    int value = monster_ai_confidence(m_ptr, feature);
    observation->value = MAX(-3, MIN(3, value + evidence));
    observation->ttl = observation_lifetime(feature);
    observation->turn = playerturn;
}

void monster_ai_witness(int feature, int evidence, int y, int x)
{
    for (int i = 1; i < mon_max; ++i)
    {
        monster_type* witness = &mon_list[i];
        if (!monster_ai_enabled(witness)
            || distance(witness->fy, witness->fx, y, x) > MAX_SIGHT
            || !los(witness->fy, witness->fx, y, x))
            continue;
        monster_ai_observe(witness, feature, evidence);
    }
}

void monster_ai_player_attack(monster_type* target, int attack_type)
{
    /* Player aggression is a social signal even when this monster cannot
     * currently learn the corresponding combat technique.  Record it before
     * the visibility and knowledge filters below. */
    if (target && target->r_idx)
        monster_social_player_attack(target);

    int feature = -1;
    switch (attack_type)
    {
        case ATT_FLANKING: feature = MON_AI_FLANKING; break;
        case ATT_CONTROLLED_RETREAT: feature = MON_AI_CONTROLLED_RETREAT; break;
        case ATT_ZONE_OF_CONTROL: feature = MON_AI_ZONE; break;
        case ATT_OPPORTUNIST: feature = MON_AI_OPPORTUNIST; break;
        case ATT_POLEARM: feature = MON_AI_POLEARM; break;
        case ATT_RIPOSTE: feature = MON_AI_RIPOSTE; break;
        case ATT_FOLLOW_THROUGH: feature = MON_AI_FOLLOW_THROUGH; break;
        case ATT_WHIRLWIND:
        case ATT_RAGE: feature = MON_AI_WHIRLWIND; break;
        case ATT_IMPALE: feature = MON_AI_IMPALE; break;
        default: break;
    }
    if (feature >= 0 && target)
        monster_ai_witness(feature, 3, target->fy, target->fx);
    if (!target) return;
    for (int i = 1; i < mon_max; ++i)
    {
        monster_type* witness = &mon_list[i];
        if (!monster_ai_enabled(witness) || !monster_ai_can_see_player(witness)
            || !los(witness->fy, witness->fx, target->fy, target->fx))
            continue;
        monster_ai_state* ai = &witness->ai;
        /* Preparation and repetition are observable patterns, not a lookup
         * of the player's active ability list or calculated attack bonus. */
        if (ai->player_action == 5 && ai->player_action_turn == playerturn - 1)
            monster_ai_observe(witness, MON_AI_FOCUS, 1);
        if (ai->attack_chain && ai->attack_turn == playerturn) continue;
        if (ai->attack_chain && ai->attack_y == target->fy
            && ai->attack_x == target->fx && playerturn >= ai->attack_turn
            && playerturn - ai->attack_turn <= 8)
            ai->attack_chain = MIN(3, ai->attack_chain + 1);
        else
            ai->attack_chain = 1;
        ai->attack_y = target->fy; ai->attack_x = target->fx;
        ai->attack_turn = playerturn;
        if (ai->attack_chain > 1)
            monster_ai_observe(witness, MON_AI_CONCENTRATION, 1);
    }
}

void monster_ai_player_action(void)
{
    int action = p_ptr->previous_action[0];
    for (int i = 1; i < mon_max; ++i)
    {
        monster_type* witness = &mon_list[i];
        if (!monster_ai_enabled(witness) || !monster_ai_can_see_player(witness))
            continue;
        monster_ai_state* ai = &witness->ai;
        if (action >= 1 && action <= 9 && action != 5
            && ai->player_action_turn == playerturn - 1
            && ((ai->player_action >= 1 && ai->player_action <= 9
                    && ai->player_action != 5)
                || ai->player_action == ACTION_ARCHERY)
            && distance(witness->fy, witness->fx, p_ptr->py, p_ptr->px)
                > distance(witness->fy, witness->fx, ai->player_y, ai->player_x))
            monster_ai_observe(witness, MON_AI_KITING, 1);
        ai->player_y = p_ptr->py; ai->player_x = p_ptr->px;
        ai->player_action = action;
        ai->player_action_turn = playerturn;
    }
}

void monster_ai_share_warning(monster_type* source)
{
    if (!monster_ai_enabled(source) || source->confused
        || source->alertness < ALERTNESS_ALERT || singing(SNG_SILENCE))
        return;
    int chosen[2] = { -1, -1 };
    int priority[2] = { 0, 0 };
    for (int feature = 0; feature < MON_AI_FEATURE_COUNT; ++feature)
    {
        /* This is personal evidence of a morale loss, not a warning that
         * the weapon threatens every allied kind. */
        if (feature == MON_AI_SLAY_FEAR) continue;
        int confidence = monster_ai_confidence(source, feature);
        int remaining = observation_remaining(&source->ai.observations[feature]);
        int strength = ABS(confidence);
        if (strength < 2 || remaining < 2)
            continue;
        int score = strength * 50 + remaining;
        if (score > priority[0])
        {
            chosen[1] = chosen[0]; priority[1] = priority[0];
            chosen[0] = feature; priority[0] = score;
        }
        else if (score > priority[1])
        {
            chosen[1] = feature; priority[1] = score;
        }
    }
    for (int i = 1; i < mon_max; ++i)
    {
        monster_type* target = &mon_list[i];
        /* A local audible warning carries knowledge, not the whole map's
         * morale effect. Solid barriers do not transmit this extra message. */
        if (target == source || !monster_ai_enabled(target) || target->confused
            || target->alertness < ALERTNESS_UNWARY
            || distance(source->fy, source->fx, target->fy, target->fx) > 12
            || !los(source->fy, source->fx, target->fy, target->fx)
            || !monster_social_allies(source, target))
            continue;
        for (int n = 0; n < 2; ++n)
        {
            int feature = chosen[n];
            if (feature < 0) continue;
            const monster_ai_observation* from = &source->ai.observations[feature];
            monster_ai_observation* to = &target->ai.observations[feature];
            int confidence = from->value > 0 ? from->value - 1 : from->value + 1;
            int remaining = observation_remaining(from) / 2;
            /* Retain the source timestamp: forwarding never creates fresh
             * evidence, and a repeated warning cannot reinforce itself. */
            if (to->ttl && to->turn >= from->turn)
                continue;
            if (ABS(monster_ai_confidence(target, feature)) >= ABS(confidence))
                continue;
            int age = playerturn - from->turn;
            to->value = confidence;
            to->turn = from->turn;
            to->ttl = MIN(observation_lifetime(feature), age + remaining);
        }
    }
}

void monster_ai_begin_turn(monster_type* m_ptr)
{
    m_ptr->ai.feedback_reason = MON_TACTIC_NONE;
    if (m_ptr->ai.feedback_cooldown) --m_ptr->ai.feedback_cooldown;
    m_ptr->ai.cast_checked = false;
    m_ptr->ai.cast_available = false;
    if (m_ptr->confused || m_ptr->skip_this_turn || m_ptr->skip_next_turn
        || m_ptr->smite_recovery || m_ptr->alertness < ALERTNESS_ALERT
        || p_ptr->truce || (singing(SNG_CHALLENGE)
            && m_ptr->stance == STANCE_AGGRESSIVE))
        m_ptr->ai.cast_reserve = 0;
    else if (m_ptr->ai.cast_reserve)
        --m_ptr->ai.cast_reserve;
    if (m_ptr->ai.goal_age) --m_ptr->ai.goal_age;
}

void monster_ai_plan_feedback(monster_type* m_ptr, int reason, int y, int x)
{
    if (!m_ptr) return;
    m_ptr->ai.feedback_reason = MON_TACTIC_NONE;
    if (!monster_ai_enabled(m_ptr) || !m_ptr->ml
        || reason <= MON_TACTIC_NONE || reason >= MON_TACTIC_MAX)
        return;
    m_ptr->ai.feedback_reason = reason;
    m_ptr->ai.feedback_y = y;
    m_ptr->ai.feedback_x = x;
}

static void monster_ai_report_action(monster_type* m_ptr, int old_y, int old_x,
    bool skipped)
{
    static s32b last_message_turn = -1;
    static const char* messages[MON_TACTIC_MAX] = {
        NULL,
        "moves out of line with its ally.",
        "spreads out from its allies.",
        "sidesteps your repeated attacks.",
        "changes its approach to your retreat.",
        "moves to guard its ranged ally.",
        "moves to cover its ally's withdrawal.",
        "draws back from your song.",
        "moves to press the attack against your singing.",
        "takes advantage of the hazardous ground.",
        "holds back, wary of your retreating strike."
    };
    int reason = m_ptr->ai.feedback_reason;
    bool moved = old_y != m_ptr->fy || old_x != m_ptr->fx;
    m_ptr->ai.feedback_reason = MON_TACTIC_NONE;
    if (!monster_ai_enabled(m_ptr) || skipped || m_ptr->ability_displaced
        || !m_ptr->ml || p_ptr->image || p_ptr->is_dead
        || m_ptr->ai.feedback_cooldown || last_message_turn == playerturn
        || reason <= MON_TACTIC_NONE || reason >= MON_TACTIC_MAX
        || m_ptr->fy != m_ptr->ai.feedback_y || m_ptr->fx != m_ptr->ai.feedback_x)
        return;
    if (reason == MON_TACTIC_RETREAT_WAIT)
    {
        if (moved || m_ptr->ability_melee
            || m_ptr->previous_action[0] == ACTION_ARCHERY) return;
    }
    else if (!moved) return;
    char name[80];
    monster_desc(name, sizeof(name), m_ptr, 0);
    msg_format("%^s %s", name, messages[reason]);
    m_ptr->ai.feedback_cooldown = 8;
    last_message_turn = playerturn;
}

void monster_ai_end_turn(monster_type* m_ptr, int old_y, int old_x, bool skipped)
{
    monster_ai_report_action(m_ptr, old_y, old_x, skipped);
    m_ptr->ai.previous_y = old_y;
    m_ptr->ai.previous_x = old_x;
    if (skipped || m_ptr->fy != old_y || m_ptr->fx != old_x
        || m_ptr->ability_melee || m_ptr->previous_action[0] == ACTION_ARCHERY)
        m_ptr->ai.waits = 0;
    else
        m_ptr->ai.waits = MIN(2, m_ptr->ai.waits + 1);
}

void monster_ai_reset(monster_type* m_ptr)
{
    memset(&m_ptr->ai, 0, sizeof(m_ptr->ai));
}

void monster_ai_sanitize(monster_type* m_ptr)
{
    for (int f = 0; f < MON_AI_FEATURE_COUNT; ++f)
    {
        monster_ai_observation* o = &m_ptr->ai.observations[f];
        o->value = MAX(-3, MIN(3, o->value));
        o->ttl = MIN(o->ttl, observation_lifetime(f));
        if (!o->value) o->ttl = 0;
    }
    m_ptr->ai.cast_reserve = MIN(3, m_ptr->ai.cast_reserve);
    m_ptr->ai.cast_checked = m_ptr->ai.cast_available = false;
    m_ptr->ai.feedback_reason = m_ptr->ai.feedback_cooldown = 0;
    m_ptr->ai.goal_age = MIN(3, m_ptr->ai.goal_age);
    m_ptr->ai.waits = MIN(2, m_ptr->ai.waits);
    monster_sense_state* sense = &m_ptr->ai.sense;
    if (sense->kind > MON_SENSE_SHARED_TRACE) memset(sense, 0, sizeof(*sense));
    sense->recent_count = MIN(4, sense->recent_count);
    sense->recent_next %= 4;
    sense->stale_decisions = MIN(8, sense->stale_decisions);
    sense->search_decisions = MIN(8, sense->search_decisions);
    if (sense->scent_age != 255)
        sense->scent_age = MIN(SMELL_STRENGTH, sense->scent_age);
    m_ptr->ai.attack_chain = MIN(3, m_ptr->ai.attack_chain);
}

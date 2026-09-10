#include "angband.h"
#include "externs.h"
#include "monster-abilities.h"

static bool has_ability(const monster_type* m_ptr, u32b ability)
{
    return m_ptr && m_ptr->r_idx > 0
        && (r_info[m_ptr->r_idx].flags5 & ability);
}

static void observe_ability(const monster_type* m_ptr, u32b ability)
{
    if (m_ptr->ml)
        l_list[m_ptr->r_idx].flags5 |= ability;
}

static bool movement_action(int action)
{
    return action >= 1 && action <= 9 && action != 5;
}

/* Keypad directions have Cartesian components in {-1,0,+1}. A positive
 * dot product means the same direction or a turn of at most 45 degrees. */
static bool compatible_direction(int first, int second)
{
    int first_x, first_y, second_x, second_y;

    if (!movement_action(first) || !movement_action(second))
        return false;
    first_x = (first - 1) % 3 - 1;
    first_y = (first - 1) / 3 - 1;
    second_x = (second - 1) % 3 - 1;
    second_y = (second - 1) / 3 - 1;
    return first_x * second_x + first_y * second_y > 0;
}

void monster_abilities_begin_action(monster_type* m_ptr)
{
    int i;

    if (!m_ptr || !m_ptr->r_idx)
        return;
    /* State 2 keeps reactions blocked after the recovery turn until this
     * creature begins another full action. State 1 still owes recovery. */
    if (m_ptr->smite_recovery == 2)
        m_ptr->smite_recovery = 0;
    for (i = ACTION_MAX - 1; i > 0; --i)
        m_ptr->previous_action[i] = m_ptr->previous_action[i - 1];
    m_ptr->previous_action[0] = ACTION_MISC;
    m_ptr->ability_in_action = true;
    m_ptr->ability_melee = false;
    m_ptr->ability_displaced = false;
}

void monster_abilities_end_action(monster_type* m_ptr, int old_y, int old_x,
    bool skipped)
{
    bool moved;
    int dy, dx;

    if (!m_ptr || !m_ptr->r_idx)
        return;
    dy = (int)m_ptr->fy - old_y;
    dx = (int)m_ptr->fx - old_x;
    moved = dy != 0 || dx != 0;

    /* Preserve the legacy movement recorder for races without these new
     * abilities; in particular, exchanges must not newly enable Charge. */
    if (r_info[m_ptr->r_idx].flags5)
    {
        /* Only completed, adjacent voluntary movement builds a run. Failed
         * moves, displacement and lost actions supply no directional step. */
        if (!skipped && !m_ptr->ability_displaced && moved
            && ABS(dy) <= 1 && ABS(dx) <= 1)
            m_ptr->previous_action[0] = (byte)(5 + dx - 3 * dy);
        else if (skipped || moved || m_ptr->ability_displaced
            || movement_action(m_ptr->previous_action[0]))
            m_ptr->previous_action[0] = ACTION_MISC;
    }

    if (has_ability(m_ptr, RF5_CONCENTRATION) && !skipped && !moved
        && !m_ptr->ability_displaced && m_ptr->ability_melee)
    {
        int cap = MAX(0, r_info[m_ptr->r_idx].per / 2);
        m_ptr->consecutive_attacks = (s16b)MIN(cap,
            MAX(0, m_ptr->consecutive_attacks) + 1);
    }
    else
        m_ptr->consecutive_attacks = 0;

    if (skipped && m_ptr->smite_recovery == 1)
        m_ptr->smite_recovery = 2;
    m_ptr->ability_in_action = false;
    m_ptr->ability_melee = false;
    m_ptr->ability_displaced = false;
}

void monster_abilities_mark_melee(monster_type* m_ptr, bool ordinary)
{
    if (m_ptr && m_ptr->ability_in_action && ordinary
        && !m_ptr->ability_displaced)
        m_ptr->ability_melee = true;
}

void monster_abilities_forced_movement(monster_type* m_ptr)
{
    int i;

    if (!m_ptr || !m_ptr->r_idx)
        return;
    for (i = 0; i < ACTION_MAX; ++i)
        m_ptr->previous_action[i] = ACTION_MISC;
    m_ptr->consecutive_attacks = 0;
    m_ptr->ability_melee = false;
    m_ptr->ability_displaced = m_ptr->ability_in_action;
}

bool monster_moved_last_action(const monster_type* m_ptr)
{
    if (!m_ptr || !m_ptr->r_idx)
        return false;
    return movement_action(
        m_ptr->previous_action[m_ptr->ability_in_action ? 1 : 0]);
}

bool monster_sprinting(const monster_type* m_ptr)
{
    int first, i;

    if (!has_ability(m_ptr, RF5_SPRINTING) || m_ptr->skip_next_turn
        || m_ptr->skip_this_turn || m_ptr->smite_recovery)
        return false;
    first = m_ptr->ability_in_action ? 1 : 0;
    if (!movement_action(m_ptr->previous_action[first]))
        return false;
    for (i = first + 1; i < first + 4; ++i)
    {
        if (!compatible_direction(m_ptr->previous_action[first],
                m_ptr->previous_action[i])
            || !compatible_direction(m_ptr->previous_action[i - 1],
                m_ptr->previous_action[i]))
            return false;
    }
    observe_ability(m_ptr, RF5_SPRINTING);
    return true;
}

bool monster_abilities_can_react(const monster_type* m_ptr)
{
    return m_ptr && m_ptr->r_idx > 0 && !m_ptr->smite_recovery
        && !m_ptr->skip_next_turn && !m_ptr->skip_this_turn
        && !m_ptr->confused && m_ptr->alertness >= ALERTNESS_ALERT;
}

int monster_concentration_bonus(const monster_type* m_ptr, bool ordinary)
{
    int bonus;

    if (!ordinary || !has_ability(m_ptr, RF5_CONCENTRATION)
        || !m_ptr->ability_in_action)
        return 0;
    bonus = MIN(MAX(0, m_ptr->consecutive_attacks),
        MAX(0, r_info[m_ptr->r_idx].per / 2));
    if (bonus)
        observe_ability(m_ptr, RF5_CONCENTRATION);
    return bonus;
}

int monster_dodging_bonus(const monster_type* m_ptr)
{
    if (!has_ability(m_ptr, RF5_DODGING) || !monster_moved_last_action(m_ptr))
        return 0;
    observe_ability(m_ptr, RF5_DODGING);
    return 3;
}

int monster_blocking_bonus_dice(const monster_type* m_ptr)
{
    int dice;

    if (!has_ability(m_ptr, RF5_BLOCKING) || monster_moved_last_action(m_ptr))
        return 0;
    /* Existing penalties remove ordinary armour dice. A completely stripped
     * shield cannot reappear just because its owner stands still. */
    dice = MIN(r_info[m_ptr->r_idx].shield_dd,
        MAX(0, r_info[m_ptr->r_idx].pd - m_ptr->song_armor_dice_penalty));
    if (dice)
        observe_ability(m_ptr, RF5_BLOCKING);
    return dice;
}

int monster_vengeance_bonus_dice(const monster_type* m_ptr)
{
    if (!has_ability(m_ptr, RF5_VENGEANCE) || !m_ptr->vengeance)
        return 0;
    observe_ability(m_ptr, RF5_VENGEANCE);
    return 1;
}

void monster_receive_melee_damage(monster_type* m_ptr, int damage)
{
    if (damage > 0 && has_ability(m_ptr, RF5_VENGEANCE))
    {
        m_ptr->vengeance = 1;
        observe_ability(m_ptr, RF5_VENGEANCE);
    }
}

void monster_consume_vengeance(monster_type* m_ptr)
{
    if (m_ptr)
        m_ptr->vengeance = 0;
}

bool monster_try_smite(monster_type* m_ptr, bool ordinary)
{
    if (!ordinary || !has_ability(m_ptr, RF5_SMITE)
        || !m_ptr->ability_in_action || m_ptr->ability_displaced
        || !monster_abilities_can_react(m_ptr)
        || m_ptr->mana < MON_MANA_COST)
        return false;
    m_ptr->mana -= MON_MANA_COST;
    m_ptr->smite_recovery = 1;
    m_ptr->skip_next_turn = true;
    observe_ability(m_ptr, RF5_SMITE);
    return true;
}

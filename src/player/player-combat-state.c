#include "angband.h"
#include "externs.h"

#define PLAYER_COMBAT_THREAT_FLOW_RANGE 20

static bool monster_is_current_combat_threat(monster_type* m_ptr)
{
    int noise_distance;

    if (!m_ptr || !m_ptr->r_idx)
        return false;
    if (p_ptr->truce || is_alert_thrall(m_ptr))
        return false;
    if (m_ptr->alertness < ALERTNESS_ALERT
        || m_ptr->stance == STANCE_FLEEING)
    {
        return false;
    }
    if (song_disguise_monster_is_fooled(m_ptr))
        return false;
    if (!(m_ptr->mflag & MFLAG_ACTV))
        return false;

    if (los(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px))
        return true;

    noise_distance = flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx);
    return noise_distance < PLAYER_COMBAT_THREAT_FLOW_RANGE;
}

bool player_in_combat(void)
{
    int i;

    if (!character_dungeon || !p_ptr || !p_ptr->playing || p_ptr->is_dead)
        return false;

    for (i = 1; i < mon_max; i++)
    {
        if (monster_is_current_combat_threat(&mon_list[i]))
            return true;
    }

    return false;
}

bool player_pack_item_action_blocked(const object_type* o_ptr)
{
    return o_ptr && o_ptr->k_idx
        && inventory_limit_group_for_object(o_ptr) == INV_LIMIT_PACK
        && player_in_combat();
}

cptr player_pack_item_action_restriction_message(void)
{
    return "You cannot use, rearrange, or pick up Pack items while in combat.";
}

bool player_pack_item_action_allowed(const object_type* o_ptr)
{
    if (!player_pack_item_action_blocked(o_ptr))
        return true;

    msg_print(player_pack_item_action_restriction_message());
    return false;
}

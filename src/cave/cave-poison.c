#include "angband.h"
#include "externs.h"

/* Exposure is contact, not an immediate Health hit. Guards prevent an entry
 * and the completion of that same action from applying two doses. */
static bool player_action_active;
static bool player_action_exposed;
static int monster_action_idx;
static bool monster_action_exposed;

/* Inspection previews resistance at the destination, before the random
 * poison-protection roll. The actual dose uses pois_dam_pure(). */
int player_poison_terrain_dose_at(int y, int x)
{
    int resistance = p_ptr->resist_pois + (p_ptr->oppose_pois ? 1 : 0);
    if (level_partition_big_cave_type_for_point(p_ptr->py, p_ptr->px) == BIG_CAVE_POIS)
        resistance++;
    if (level_partition_big_cave_type_for_point(y, x) == BIG_CAVE_POIS)
        resistance--;
    if (resistance < 1)
        return POISON_TERRAIN_DOSE * (2 - resistance);
    return POISON_TERRAIN_DOSE * 2 / (resistance + 1);
}

void player_poison_terrain_begin_action(void)
{
    player_action_active = true;
    player_action_exposed = false;
}

void player_poison_terrain_exposure(bool airborne)
{
    if (!p_ptr || p_ptr->is_dead || airborne
        || !in_bounds(p_ptr->py, p_ptr->px)
        || cave_feat[p_ptr->py][p_ptr->px] != FEAT_POISON
        || (player_action_active && player_action_exposed))
        return;
    if (player_action_active)
        player_action_exposed = true;
    pois_dam_pure(POISON_TERRAIN_DOSE, 1, false);
}

void player_poison_terrain_end_action(void)
{
    if (p_ptr->energy_use && !player_action_exposed)
        player_poison_terrain_exposure(p_ptr->leaping);
    player_action_active = false;
}

void monster_poison_terrain_exposure(int m_idx)
{
    monster_type* m_ptr;
    monster_race* r_ptr;
    if (m_idx <= 0 || m_idx >= mon_max)
        return;
    m_ptr = &mon_list[m_idx];
    if (!m_ptr->r_idx || cave_feat[m_ptr->fy][m_ptr->fx] != FEAT_POISON
        || (monster_action_idx == m_idx && monster_action_exposed))
        return;
    r_ptr = &r_info[m_ptr->r_idx];
    if (r_ptr->flags2 & RF2_FLYING)
        return;
    if (monster_action_idx == m_idx)
        monster_action_exposed = true;
    monster_poison_add(m_idx, POISON_TERRAIN_DOSE);
}

void monster_poison_terrain_begin_action(int m_idx)
{
    monster_action_idx = m_idx;
    monster_action_exposed = false;
    monster_poison_terrain_exposure(m_idx);
}

void monster_poison_terrain_end_action(int m_idx)
{
    if (monster_action_idx == m_idx)
    {
        monster_action_idx = 0;
        monster_action_exposed = false;
    }
}

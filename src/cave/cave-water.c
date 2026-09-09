#include "angband.h"
#include "externs.h"

/* Apply once to a completed movement, including the step back onto a bank.
 * Stationary actions and airborne crossings do not touch the surface. */
int water_movement_energy(int energy, int from_feat, int to_feat, bool airborne)
{
    if (!airborne && (from_feat == FEAT_WATER || to_feat == FEAT_WATER))
        return energy + (energy + 1) / 2;
    return energy;
}

void player_water_movement(int from_feat, int to_feat)
{
    if (from_feat != FEAT_WATER && to_feat != FEAT_WATER)
        return;
    p_ptr->energy_use = water_movement_energy(
        p_ptr->energy_use, from_feat, to_feat, false);
    stealth_score -= WATER_STEALTH_PENALTY;
    if (to_feat == FEAT_WATER && !p_ptr->diseased
        && one_in_(DISEASE_WATER_ONE_IN))
        (void)infect_disease();
}

/* Forced movement spends the instigator's action, not another player turn.
 * Make its splash audible now; the next action resets the Stealth score. */
void player_water_displaced(int from_feat, int to_feat)
{
    if (from_feat != FEAT_WATER && to_feat != FEAT_WATER)
        return;
    update_flow(p_ptr->py, p_ptr->px, FLOW_PLAYER_NOISE);
    if (to_feat == FEAT_WATER && !p_ptr->diseased
        && one_in_(DISEASE_WATER_ONE_IN))
        (void)infect_disease();
    monster_perception(true, false, p_ptr->skill_use[S_STL] - WATER_STEALTH_PENALTY);
}

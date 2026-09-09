#include "angband.h"
#include "externs.h"

/* Only the impacted surface changes. Resistance and armour belong to the
 * occupant and cannot protect water or ice from an elemental attack. */
bool cave_transform_elemental_terrain(int y, int x, int typ)
{
    int feat;
    if (!in_bounds(y, x))
        return false;
    feat = cave_feat[y][x];
    if (typ == GF_FIRE && feat == FEAT_ICE)
        cave_set_feat(y, x, FEAT_WATER);
    else if (typ == GF_COLD && feat == FEAT_WATER)
        cave_set_feat(y, x, FEAT_ICE);
    else
        return false;
    return true;
}

/* Combine a bow and arrow before transforming: one hit changes the original
 * surface at most once, even when its equipment carries both brands. */
void cave_apply_elemental_brands(int y, int x,
    const object_type* weapon, const object_type* ammunition)
{
    u32b brands = 0, f1, f2, f3;
    if (!in_bounds(y, x))
        return;
    if (weapon && weapon->k_idx)
    {
        object_flags(weapon, &f1, &f2, &f3);
        brands |= f1;
    }
    if (ammunition && ammunition->k_idx)
    {
        object_flags(ammunition, &f1, &f2, &f3);
        brands |= f1;
    }
    if (cave_feat[y][x] == FEAT_ICE && (brands & TR1_BRAND_FIRE))
        (void)cave_transform_elemental_terrain(y, x, GF_FIRE);
    else if (cave_feat[y][x] == FEAT_WATER && (brands & TR1_BRAND_COLD))
        (void)cave_transform_elemental_terrain(y, x, GF_COLD);
}

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

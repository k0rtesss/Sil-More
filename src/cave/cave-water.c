#include "angband.h"
#include "externs.h"

/* Action guards prevent entering ice and finishing that same action from
 * rolling twice. Forced displacement outside an action still makes contact. */
static bool ice_player_active, ice_player_checked;
static int ice_monster_active;
static bool ice_monster_checked;

bool cave_deep_water_allowed(int y, int x)
{
    if (!in_bounds_fully(y, x)) return false;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
        {
            if (!dy && !dx) continue;
            int feat = cave_feat[y + dy][x + dx];
            /* Bridges are dry ground even when their underlay is water. */
            if (feat != FEAT_WATER && feat != FEAT_DEEP_WATER
                && !FEAT_IS_ICE(feat))
                return false;
        }
    return true;
}

static int melted_ice_feature(int y, int x)
{
    return cave_deep_water_allowed(y, x) && one_in_(2)
        ? FEAT_DEEP_WATER : FEAT_WATER;
}

bool player_melting_ice_exposure(void)
{
    if (!p_ptr || p_ptr->is_dead || p_ptr->leaving || p_ptr->leaping
        || !in_bounds(p_ptr->py, p_ptr->px)
        || cave_feat[p_ptr->py][p_ptr->px] != FEAT_MELTING_ICE
        || (ice_player_active && ice_player_checked))
        return false;
    if (ice_player_active) ice_player_checked = true;
    if (!one_in_(MELTING_ICE_BREAK_ONE_IN)) return false;
    int feat = melted_ice_feature(p_ptr->py, p_ptr->px);
    cave_set_feat(p_ptr->py, p_ptr->px, feat);
    disturb(0, 0);
    msg_print(feat == FEAT_DEEP_WATER
        ? "The ice breaks beneath you! You plunge into deep water!"
        : "The ice breaks beneath you! You splash into shallow water.");
    return true;
}

void player_melting_ice_begin_action(void)
{
    ice_player_active = true;
    ice_player_checked = false;
}

void player_melting_ice_end_action(void)
{
    if (p_ptr->energy_use && player_melting_ice_exposure())
        player_water_displaced(FEAT_MELTING_ICE, cave_feat[p_ptr->py][p_ptr->px]);
    ice_player_active = false;
}

void monster_melting_ice_exposure(int m_idx)
{
    if (m_idx <= 0 || m_idx >= mon_max) return;
    monster_type* m_ptr = &mon_list[m_idx];
    if (!m_ptr->r_idx || !in_bounds(m_ptr->fy, m_ptr->fx)
        || cave_feat[m_ptr->fy][m_ptr->fx] != FEAT_MELTING_ICE
        || (r_info[m_ptr->r_idx].flags2 & RF2_FLYING)
        || (ice_monster_active == m_idx && ice_monster_checked))
        return;
    if (ice_monster_active == m_idx) ice_monster_checked = true;
    if (!one_in_(MELTING_ICE_BREAK_ONE_IN)) return;
    int feat = melted_ice_feature(m_ptr->fy, m_ptr->fx);
    cave_set_feat(m_ptr->fy, m_ptr->fx, feat);
    if (m_ptr->ml)
    {
        char name[80];
        monster_desc(name, sizeof(name), m_ptr, 0);
        msg_format("The ice breaks beneath %s!", name);
    }
}

void monster_melting_ice_begin_action(int m_idx)
{
    ice_monster_active = m_idx;
    ice_monster_checked = false;
    monster_melting_ice_exposure(m_idx);
}

void monster_melting_ice_end_action(int m_idx)
{
    if (ice_monster_active == m_idx)
    {
        ice_monster_active = 0;
        ice_monster_checked = false;
    }
}

bool player_submerged_in_deep_water(void)
{
    return p_ptr && in_bounds(p_ptr->py, p_ptr->px) && !p_ptr->leaping
        && cave_feat[p_ptr->py][p_ptr->px] == FEAT_DEEP_WATER;
}

/* Only the impacted surface changes. Resistance and armour belong to the
 * occupant and cannot protect water or ice from an elemental attack. */
bool cave_transform_elemental_terrain(int y, int x, int typ)
{
    int feat;
    if (!in_bounds(y, x))
        return false;
    feat = cave_feat[y][x];
    if (typ == GF_FIRE && FEAT_IS_ICE(feat))
        cave_set_feat(y, x, feat == FEAT_MELTING_ICE
            ? melted_ice_feature(y, x) : FEAT_WATER);
    else if (typ == GF_COLD && feat == FEAT_MELTING_ICE)
        cave_set_feat(y, x, FEAT_ICE);
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
    if (FEAT_IS_ICE(cave_feat[y][x]) && (brands & TR1_BRAND_FIRE))
        (void)cave_transform_elemental_terrain(y, x, GF_FIRE);
    else if ((cave_feat[y][x] == FEAT_WATER
        || cave_feat[y][x] == FEAT_MELTING_ICE) && (brands & TR1_BRAND_COLD))
        (void)cave_transform_elemental_terrain(y, x, GF_COLD);
}

/* Apply once to a completed movement, including the step back onto a bank.
 * Stationary actions and airborne crossings do not touch the surface. */
int water_movement_energy(int energy, int from_feat, int to_feat, bool airborne)
{
    if (!airborne && (from_feat == FEAT_DEEP_WATER || to_feat == FEAT_DEEP_WATER))
        return energy * 4;
    if (!airborne && (from_feat == FEAT_WATER || to_feat == FEAT_WATER))
        return energy + (energy + 1) / 2;
    return energy;
}

/* Return the movement energy for a grounded step while standing on the
 * player's current terrain. Keep the status display tied to the same
 * movement-cost function used by actual player movement. */
int player_current_movement_energy(void)
{
    int feat;

    if (!p_ptr || !in_bounds(p_ptr->py, p_ptr->px))
        return 100;

    feat = cave_feat[p_ptr->py][p_ptr->px];
    return water_movement_energy(100, feat, feat, p_ptr->leaping);
}

/* Convert the current movement cost into the speed scale used by the player
 * and monster energy tables. Shallow water removes one speed step; deep
 * water removes two. This is presentation only: p_ptr->pspeed itself is
 * unchanged, so terrain never slows stationary actions or attacks. */
int player_current_movement_speed(void)
{
    int speed = p_ptr ? p_ptr->pspeed : 2;
    int energy = player_current_movement_energy();

    if (energy >= 400)
        speed -= 2;
    else if (energy > 100)
        speed -= 1;

    return speed;
}

void player_water_movement(int from_feat, int to_feat)
{
    if (from_feat != FEAT_WATER && to_feat != FEAT_WATER
        && from_feat != FEAT_DEEP_WATER && to_feat != FEAT_DEEP_WATER)
        return;
    p_ptr->energy_use = water_movement_energy(
        p_ptr->energy_use, from_feat, to_feat, false);
    p_ptr->redraw |= PR_SPEED;
    stealth_score -= WATER_STEALTH_PENALTY;
    if ((to_feat == FEAT_WATER || to_feat == FEAT_DEEP_WATER) && !p_ptr->diseased
        && one_in_(DISEASE_WATER_ONE_IN))
        (void)infect_disease();
}

/* Forced movement spends the instigator's action, not another player turn.
 * Make its splash audible now; the next action resets the Stealth score. */
void player_water_displaced(int from_feat, int to_feat)
{
    if (from_feat != FEAT_WATER && to_feat != FEAT_WATER
        && from_feat != FEAT_DEEP_WATER && to_feat != FEAT_DEEP_WATER)
        return;
    p_ptr->redraw |= PR_SPEED;
    update_flow(p_ptr->py, p_ptr->px, FLOW_PLAYER_NOISE);
    if ((to_feat == FEAT_WATER || to_feat == FEAT_DEEP_WATER) && !p_ptr->diseased
        && one_in_(DISEASE_WATER_ONE_IN))
        (void)infect_disease();
    monster_perception(true, false, p_ptr->skill_use[S_STL] - WATER_STEALTH_PENALTY);
}

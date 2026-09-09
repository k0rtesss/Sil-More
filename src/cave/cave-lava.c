#include "angband.h"
#include "externs.h"
#include "player/killer.h"

/* These guards last only for the currently executing action, never a save.
 * Entry is immediate; completing the same action must not charge it twice. */
static bool player_action_active;
static bool player_action_exposed;
static int monster_action_idx;
static bool monster_action_exposed;

/* Return -1 for lethal ground contact. Use the existing elemental curve:
 * 60 * 2 / (2 + net resistance stacks). Flight adds one stack before
 * vulnerability is applied, and always receives heat rather than auto-death. */
static int lava_damage_at_resistance(int resistance, bool airborne)
{
    if (airborne) resistance++;
    if (!airborne && resistance <= 1)
        return -1;
    if (resistance < 1)
        return LAVA_RAW_DAMAGE * (2 - resistance);
    return MAX(1, (LAVA_RAW_DAMAGE * 2) / (resistance + 1));
}

int player_lava_damage(bool airborne)
{
    return lava_damage_at_resistance(p_ptr->resist_fire
        + (p_ptr->oppose_fire ? 1 : 0), airborne);
}

int player_lava_damage_at(int y, int x, bool airborne)
{
    int resistance = p_ptr->resist_fire + (p_ptr->oppose_fire ? 1 : 0);
    /* Bonuses currently describe the occupied square; previews must use
     * the fire-cave penalty at the destination instead. */
    if (level_partition_big_cave_type_for_point(p_ptr->py, p_ptr->px) == BIG_CAVE_FIRE)
        resistance++;
    if (level_partition_big_cave_type_for_point(y, x) == BIG_CAVE_FIRE)
        resistance--;
    return lava_damage_at_resistance(resistance, airborne);
}

void player_lava_begin_action(void)
{
    player_action_active = true;
    player_action_exposed = false;
}

void player_lava_exposure(bool airborne)
{
    int damage;
    if (!p_ptr || p_ptr->is_dead || !in_bounds(p_ptr->py, p_ptr->px)
        || cave_feat[p_ptr->py][p_ptr->px] != FEAT_LAVA)
        return;
    if (player_action_active)
        player_action_exposed = true;
    damage = player_lava_damage(airborne);
    killer_mark_other(SCORE_KILLER_OTHER);
    if (damage < 0)
    {
        msg_print("The molten lava consumes you!");
        take_hit(MAX(1, p_ptr->chp), "molten lava");
    }
    else
    {
        msg_format(airborne ? "The heat of the lava burns you for %d damage!"
                            : "The lava burns you for %d damage!", damage);
        take_hit(damage, "molten lava");
        ident_resist(TR2_RES_FIRE);
    }
}

void player_lava_end_action(void)
{
    if (p_ptr->energy_use && !player_action_exposed)
        player_lava_exposure(p_ptr->leaping);
    player_action_active = false;
}

bool monster_lava_exposure(int m_idx)
{
    monster_type* m_ptr;
    monster_race* r_ptr;
    char name[80];
    bool flying;
    if (m_idx <= 0 || m_idx >= mon_max)
        return false;
    m_ptr = &mon_list[m_idx];
    if (!m_ptr->r_idx)
        return true;
    if (cave_feat[m_ptr->fy][m_ptr->fx] != FEAT_LAVA)
        return false;
    r_ptr = &r_info[m_ptr->r_idx];
    if (r_ptr->flags3 & RF3_RES_FIRE)
    {
        if (m_ptr->ml) l_list[m_ptr->r_idx].flags3 |= RF3_RES_FIRE;
        return false;
    }
    if (monster_action_idx == m_idx && monster_action_exposed)
        return false;
    if (monster_action_idx == m_idx)
        monster_action_exposed = true;
    flying = (r_ptr->flags2 & RF2_FLYING) != 0;
    if (m_ptr->ml)
    {
        monster_desc(name, sizeof(name), m_ptr, 0);
        msg_format(flying ? "%^s is scorched by the lava's heat!"
                          : "%^s is consumed by the lava!", name);
        if (flying) l_list[m_ptr->r_idx].flags2 |= RF2_FLYING;
    }
    /* Like chasm deaths, terrain can kill uniques. Resolve drops, experience
     * and race bookkeeping through the ordinary death routine first. */
    if (!flying || m_ptr->hp <= LAVA_FLYING_DAMAGE)
    {
        monster_death(m_idx);
        delete_monster_idx(m_idx);
        return true;
    }
    m_ptr->hp -= LAVA_FLYING_DAMAGE;
    if (p_ptr->health_who == m_idx) p_ptr->redraw |= PR_HEALTHBAR;
    return false;
}

bool monster_lava_begin_action(int m_idx)
{
    monster_action_idx = m_idx;
    monster_action_exposed = false;
    return monster_lava_exposure(m_idx);
}

void monster_lava_end_action(int m_idx)
{
    if (monster_action_idx == m_idx)
    {
        monster_action_idx = 0;
        monster_action_exposed = false;
    }
}

/* Lava contributes a radius-two light field. Take the brightest source,
 * rather than stacking every pool tile into unlimited darkness resistance.
 * Recompute from terrain each view update so removal and loading work too. */
void lava_light(void)
{
    byte light[MAX_DUNGEON_HGT][MAX_DUNGEON_WID] = { { 0 } };
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
        for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
        {
            if (cave_feat[y][x] != FEAT_LAVA) continue;
            for (int dy = -2; dy <= 2; dy++)
                for (int dx = -2; dx <= 2; dx++)
                {
                    int yy = y + dy, xx = x + dx;
                    int d = distance(y, x, yy, xx);
                    if (!in_bounds(yy, xx) || d > 2 || !los(y, x, yy, xx))
                        continue;
                    light[yy][xx] = MAX(light[yy][xx], 3 - d);
                }
        }
    for (int y = 0; y < p_ptr->cur_map_hgt; y++)
        for (int x = 0; x < p_ptr->cur_map_wid; x++)
            cave_light[y][x] += light[y][x];
}

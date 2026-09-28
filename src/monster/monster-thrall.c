#include "angband.h"
#include "externs.h"
#include "monster/monster-senses.h"

/* Slow speed plus one wandering/inspection action in eight: about 16 normal
 * player turns. Miners discover quartz only by checking adjacent walls. */
#define THRALL_WORK_ONE_IN 8
#define THRALLMASTER_KILL_ONE_IN 250

static bool is_mining_thrall(const monster_type* m_ptr)
{
    return m_ptr->r_idx == R_IDX_HUMAN_THRALL
        || m_ptr->r_idx == R_IDX_ELF_THRALL;
}

static bool thrall_can_walk(int y, int x)
{
    return in_bounds_fully(y, x) && cave_empty_bold(y, x)
        && !cave_trap_bold(y, x) && !cave_glyph(y, x)
        && cave_feat[y][x] != FEAT_DEEP_WATER
        && cave_feat[y][x] != FEAT_ILLUSORY_WALL;
}

static void thrall_wander(monster_type* m_ptr)
{
    int choices[8], count = 0;
    int oy = m_ptr->fy, ox = m_ptr->fx;

    /* Choose a nearby step or wall to inspect without favouring quartz.
     * Ordinary stone costs an inspection too, then the miner wanders on. */
    for (int i = 0; i < 8; i++)
    {
        int y = oy + ddy_ddd[i], x = ox + ddx_ddd[i];
        if (!in_bounds_fully(y, x) || cave_m_idx[y][x])
            continue;
        int feat = cave_feat[y][x];
        if (thrall_can_walk(y, x)
            || FEAT_IS_WALL(feat)
            || feat == FEAT_ILLUSORY_WALL)
            choices[count++] = i;
    }
    if (!count)
        return;

    int dir = choices[rand_int(count)];
    int y = oy + ddy_ddd[dir], x = ox + ddx_ddd[dir];
    if (thrall_can_walk(y, x))
    {
        monster_swap(oy, ox, y, x);
        if (m_ptr->r_idx && m_ptr->fy == y && m_ptr->fx == x)
        {
            m_ptr->previous_action[0] = rough_direction(oy, ox, y, x);
            m_ptr->energy -= water_movement_energy(100,
                cave_feat[oy][ox], cave_feat[y][x], false) - 100;
        }
        return;
    }

    m_ptr->visual_facing_dir = (byte)rough_direction(oy, ox, y, x);
    if (m_ptr->ml)
        lite_spot(oy, ox);
    if (FEAT_IS_QUARTZ(cave_feat[y][x]))
    {
        cave_set_feat(y, x, FEAT_RUBBLE);
        p_ptr->update |= PU_UPDATE_VIEW | PU_MONSTERS;
        if (m_ptr->ml && player_can_see_bold(y, x))
        {
            char name[80];
            monster_desc(name, sizeof(name), m_ptr, 0);
            msg_format("%^s chips the quartz into rubble.", name);
        }
    }
}

static bool thrallmaster_attack(monster_type* m_ptr)
{
    int ty, tx;
    int victim = 0, count = 0;
    object_type skeleton;

    /* Fighting or fleeing overseers attend to their own survival. */
    if (m_ptr->stance == STANCE_FLEEING)
        return false;
    monster_senses_refresh(m_ptr);
    if (monster_senses_target(m_ptr, &ty, &tx))
        return false;

    for (int i = 0; i < 8; i++)
    {
        int y = m_ptr->fy + ddy_ddd[i];
        int x = m_ptr->fx + ddx_ddd[i];
        if (!in_bounds_fully(y, x) || cave_m_idx[y][x] <= 0)
            continue;
        int idx = cave_m_idx[y][x];
        /* Match the two dejected races, never the quest-giver races. */
        if (is_mining_thrall(&mon_list[idx]))
        {
            count++;
            if (one_in_(count))
                victim = idx;
        }
    }
    if (!victim || !one_in_(THRALLMASTER_KILL_ONE_IN))
        return false;

    monster_type* thrall = &mon_list[victim];
    int y = thrall->fy, x = thrall->fx;
    int sval = thrall->r_idx == R_IDX_ELF_THRALL
        ? SV_SKELETON_ELF : SV_SKELETON_HUMAN;
    int kind = lookup_kind(TV_SKELETON, sval);
    if (!kind)
        return false;

    if (m_ptr->ml && thrall->ml)
    {
        char master_name[80], thrall_name[80];
        monster_desc(master_name, sizeof(master_name), m_ptr, 0);
        monster_desc(thrall_name, sizeof(thrall_name), thrall, 0);
        msg_format("%^s strikes %s down.", master_name, thrall_name);
    }
    else if (thrall->ml)
    {
        char name[80];
        monster_desc(name, sizeof(name), thrall, 0);
        msg_format("%^s is struck down.", name);
    }

    m_ptr->visual_facing_dir = (byte)rough_direction(m_ptr->fy, m_ptr->fx, y, x);
    monster_sound(thrall, MONSTER_SOUND_DEATH);
    /* This ambient death must not award player kills/XP or fail mercy quests.
     * Dejected thralls carry no loot; the remains are an ordinary skeleton. */
    delete_monster_idx(victim);
    object_wipe(&skeleton);
    object_prep(&skeleton, kind);
    skeleton.pval = 1;
    drop_near(&skeleton, -1, y, x);
    p_ptr->window |= PW_MONLIST | PW_MONSTER;
    return true;
}

/* True means the ambient behaviour consumed this creature's action. */
bool monster_thrall_turn(monster_type* m_ptr)
{
    bool miner = is_mining_thrall(m_ptr);
    if (!miner && m_ptr->r_idx != R_IDX_ORC_THRALLMASTER)
        return false;
    if (m_ptr->alertness < ALERTNESS_UNWARY || m_ptr->confused
        || m_ptr->stunned || m_ptr->skip_this_turn)
        return miner;
    if (!miner)
        return thrallmaster_attack(m_ptr);

    if (one_in_(THRALL_WORK_ONE_IN))
        thrall_wander(m_ptr);
    return true;
}

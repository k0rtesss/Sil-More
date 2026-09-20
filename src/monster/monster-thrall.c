#include "angband.h"
#include "externs.h"
#include "monster/monster-senses.h"

/* Slow speed plus one working action in eight: about 16 normal player turns.
 * Search only the local mine, and recompute routes as miners change it. */
#define THRALL_WORK_ONE_IN 8
#define THRALL_SEARCH_RADIUS 12
#define THRALL_SEARCH_WIDTH (2 * THRALL_SEARCH_RADIUS + 1)
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
        && cave_feat[y][x] != FEAT_DEEP_WATER;
}

static void thrall_mine(monster_type* m_ptr)
{
    /* Breadth-first search finds a reachable working face, even round corners.
     * Each entry remembers its first step; no persistent target is needed. */
    struct thrall_step { int y, x, first, distance; };
    struct thrall_step queue[THRALL_SEARCH_WIDTH * THRALL_SEARCH_WIDTH];
    bool visited[THRALL_SEARCH_WIDTH][THRALL_SEARCH_WIDTH] = { { false } };
    int head = 0, tail = 1;
    int oy = m_ptr->fy, ox = m_ptr->fx;
    int start = rand_int(8);

    queue[0] = (struct thrall_step){ oy, ox, -1, 0 };
    visited[THRALL_SEARCH_RADIUS][THRALL_SEARCH_RADIUS] = true;

    while (head < tail)
    {
        struct thrall_step step = queue[head++];
        for (int i = 0; i < 8; i++)
        {
            int dir = (start + i) % 8;
            int y = step.y + ddy_ddd[dir];
            int x = step.x + ddx_ddd[dir];
            int vy = y - oy + THRALL_SEARCH_RADIUS;
            int vx = x - ox + THRALL_SEARCH_RADIUS;

            if (!in_bounds_fully(y, x))
                continue;

            if (cave_feat[y][x] == FEAT_QUARTZ && !cave_m_idx[y][x])
            {
                if (step.first < 0)
                {
                    m_ptr->visual_facing_dir = (byte)rough_direction(oy, ox, y, x);
                    cave_set_feat(y, x, FEAT_RUBBLE);
                    p_ptr->update |= PU_UPDATE_VIEW | PU_MONSTERS;
                    if (m_ptr->ml && player_can_see_bold(y, x))
                    {
                        char name[80];
                        monster_desc(name, sizeof(name), m_ptr, 0);
                        msg_format("%^s chips the quartz into rubble.", name);
                    }
                }
                else
                {
                    int ny = oy + ddy_ddd[step.first];
                    int nx = ox + ddx_ddd[step.first];
                    monster_swap(oy, ox, ny, nx);
                    if (m_ptr->r_idx && m_ptr->fy == ny && m_ptr->fx == nx)
                    {
                        m_ptr->previous_action[0] = rough_direction(oy, ox, ny, nx);
                        m_ptr->energy -= water_movement_energy(100,
                            cave_feat[oy][ox], cave_feat[ny][nx], false) - 100;
                    }
                }
                return;
            }

            if (step.distance >= THRALL_SEARCH_RADIUS
                || vy < 0 || vy >= THRALL_SEARCH_WIDTH
                || vx < 0 || vx >= THRALL_SEARCH_WIDTH
                || visited[vy][vx] || !thrall_can_walk(y, x))
                continue;

            visited[vy][vx] = true;
            queue[tail++] = (struct thrall_step){ y, x,
                step.first < 0 ? dir : step.first, step.distance + 1 };
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
        thrall_mine(m_ptr);
    return true;
}

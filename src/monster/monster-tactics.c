#include "angband.h"
#include "externs.h"
#include "monster-tactics.h"

int monster_ai_poison_preview(int pending, bool pool_contact, bool entry,
    int actions, int* remaining)
{
    int damage = 0;
    pending = MIN(100, MAX(0, pending));
    if (pool_contact && entry)
        pending = MIN(100, pending + POISON_TERRAIN_DOSE);
    for (int i = 0; i < actions; ++i)
    {
        int tick;
        if (pool_contact)
            pending = MIN(100, pending + POISON_TERRAIN_DOSE);
        tick = (pending + 4) / 5;
        pending -= tick;
        damage += tick;
    }
    if (remaining)
        *remaining = pending;
    return damage;
}

int monster_ai_poison_damage(const monster_type* m_ptr, int y, int x, int actions)
{
    const monster_race* r_ptr;
    bool contact, entry;
    if (!m_ptr || !m_ptr->r_idx || !in_bounds(y, x))
        return 0;
    r_ptr = &r_info[m_ptr->r_idx];
    contact = cave_feat[y][x] == FEAT_POISON
        && !(r_ptr->flags2 & RF2_FLYING) && !(r_ptr->flags3 & RF3_RES_POIS);
    /* An action that began in a pool already received its contact dose;
     * moving to another pool cell does not apply that dose again. */
    entry = (y != m_ptr->fy || x != m_ptr->fx)
        && cave_feat[m_ptr->fy][m_ptr->fx] != FEAT_POISON;
    return monster_ai_poison_preview(m_ptr->poisoned, contact, entry,
        actions, NULL);
}

bool monster_ai_poison_safe(const monster_type* m_ptr, int y, int x, int actions)
{
    if (!m_ptr || !m_ptr->r_idx || !in_bounds(y, x))
        return false;
    return monster_ai_poison_damage(m_ptr, y, x, actions) < m_ptr->hp;
}

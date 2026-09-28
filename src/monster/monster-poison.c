#include "angband.h"
#include "externs.h"

/* Poison is pending damage, with the same cap and decay as player poison. */
void monster_poison_add(int m_idx, int amount)
{
    monster_type* m_ptr;

    if (m_idx <= 0 || m_idx >= mon_max || amount <= 0)
        return;
    m_ptr = &mon_list[m_idx];
    if (!m_ptr->r_idx)
        return;
    if (r_info[m_ptr->r_idx].flags3 & RF3_RES_POIS)
    {
        if (m_ptr->ml)
            l_list[m_ptr->r_idx].flags3 |= RF3_RES_POIS;
        return;
    }

    m_ptr->poisoned = MIN(100, m_ptr->poisoned + MIN(amount, 100));
    m_ptr->mflag |= MFLAG_ACTV;
    if (p_ptr->health_who == m_idx)
        p_ptr->redraw |= PR_HEALTHBAR;
    p_ptr->window |= PW_MONLIST | PW_MONSTER;
}

/* Called once per monster action, including skipped or sleeping actions. */
bool monster_poison_tick(int m_idx)
{
    monster_type* m_ptr;
    int damage;
    bool seen;
    int y, x;

    if (m_idx <= 0 || m_idx >= mon_max)
        return false;
    m_ptr = &mon_list[m_idx];
    if (!m_ptr->r_idx || m_ptr->poisoned <= 0)
        return false;

    damage = (m_ptr->poisoned + 4) / 5;
    m_ptr->poisoned -= damage;
    seen = m_ptr->ml;
    y = m_ptr->fy;
    x = m_ptr->fx;
    p_ptr->window |= PW_MONLIST | PW_MONSTER;

    /* The ordinary damage route preserves Morgoth's transition and drops. */
    bool dead = mon_take_hit(m_idx, damage,
        seen ? " dies of poisoning." : "", 0);
    if (seen)
        display_hit(y, x, damage, GF_POIS, dead);
    return dead;
}

/* Bow and ammunition brands supply one dose together, rather than doubling. */
void monster_poison_brand(int m_idx, const object_type* first,
    const object_type* second, int damage)
{
    u32b f1, f2, f3;
    bool branded = false;

    if (damage <= 0)
        return;
    if (first)
    {
        object_flags(first, &f1, &f2, &f3);
        branded = (f1 & TR1_BRAND_POIS) != 0;
    }
    if (second)
    {
        object_flags(second, &f1, &f2, &f3);
        branded |= (f1 & TR1_BRAND_POIS) != 0;
    }
    if (branded)
        monster_poison_add(m_idx, (damage + 1) / 2);
}

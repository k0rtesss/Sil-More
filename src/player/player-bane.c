/* File: player/player-bane.c */

#include "angband.h"

#include "player/player-bane.h"

static u32b bane_flag[] = { 0L, RF3_ORC, RF3_WOLF, RF3_SPIDER, RF3_TROLL,
    RF3_UNDEAD, RF3_RAUKO, RF3_SERPENT, RF3_DRAGON, RF3_VAMPIRE,
    RF3_HORROR, RF3_CAT, RF3_GIANT };

char* bane_name[] = { "Nothing", "Orc", "Wolf", "Spider", "Troll", "Wraith",
    "Rauko", "Serpent", "Dragon", "Vampire", "Horror", "Cat", "Giant" };

int bane_type_killed(int bane_type)
{
    int j;
    int count = 0;

    for (j = 1; j < z_info->r_max; j++)
    {
        monster_race* r_ptr = &r_info[j];
        monster_lore* l_ptr = &l_list[j];

        if (r_ptr->flags3 & bane_flag[bane_type])
            count += l_ptr->pkills;
    }

    return count;
}

static int bane_bonus_aux(void)
{
    int threshold = 2;
    int bonus = 0;
    int killed = bane_type_killed(p_ptr->bane_type);

    while (threshold <= killed)
    {
        threshold *= 2;
        bonus++;
    }

    return bonus;
}

int elf_bane_bonus(monster_type* m_ptr)
{
    monster_race* r_ptr;

    if (m_ptr == NULL)
        return 0;

    r_ptr = &r_info[m_ptr->r_idx];

    if ((r_ptr->flags2 & RF2_ELFBANE)
        && ((p_ptr->prace == 0) || (p_ptr->prace == 1)))
    {
        return 5;
    }

    return 0;
}

int bane_bonus(monster_type* m_ptr)
{
    monster_race* r_ptr;

    if (m_ptr == NULL)
        return 0;

    if (p_ptr->entranced)
        return 0;

    if (p_ptr->stun > 100)
        return 0;

    r_ptr = &r_info[m_ptr->r_idx];

    if (r_ptr->flags3 & bane_flag[p_ptr->bane_type])
        return bane_bonus_aux();

    return 0;
}

int bane_bonus_for_type(int bane_type_idx)
{
    int threshold = 2;
    int bonus = 0;
    int killed;

    if (bane_type_idx <= 0 || bane_type_idx >= PLAYER_BANE_TYPES)
        return 0;

    killed = bane_type_killed(bane_type_idx);
    while (threshold <= killed)
    {
        threshold *= 2;
        bonus++;
    }

    return bonus;
}

int artifact_bane_bonus(monster_type* m_ptr)
{
    int bonus = 0;
    int i, j;
    monster_race* r_ptr;

    if (m_ptr == NULL)
        return 0;

    if (p_ptr->entranced)
        return 0;

    if (p_ptr->stun > 100)
        return 0;

    r_ptr = &r_info[m_ptr->r_idx];

    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        object_type* o_ptr = &inventory[i];

        if (!o_ptr->k_idx)
            continue;

        for (j = 0; j < o_ptr->abilities; j++)
        {
            if (o_ptr->skilltype[j] == S_PER && o_ptr->abilitynum[j] == PER_BANE
                && o_ptr->bane_type[j] > 0)
            {
                if (o_ptr->bane_type[j] == p_ptr->bane_type)
                    continue;

                if (r_ptr->flags3 & bane_flag[o_ptr->bane_type[j]])
                {
                    int this_bonus = bane_bonus_for_type(o_ptr->bane_type[j]);
                    if (this_bonus > bonus)
                        bonus = this_bonus;
                }
            }
        }
    }

    return bonus;
}

int spider_bane_bonus(void)
{
    if (bane_flag[p_ptr->bane_type] == RF3_SPIDER)
        return bane_bonus_aux();

    return 0;
}

int artifact_spider_bane_bonus(void)
{
    int bonus = 0;
    int i, j;

    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        object_type* o_ptr = &inventory[i];

        if (!o_ptr->k_idx)
            continue;

        for (j = 0; j < o_ptr->abilities; j++)
        {
            if (o_ptr->skilltype[j] == S_PER && o_ptr->abilitynum[j] == PER_BANE
                && bane_flag[o_ptr->bane_type[j]] == RF3_SPIDER)
            {
                if (o_ptr->bane_type[j] == p_ptr->bane_type)
                    continue;

                {
                    int this_bonus = bane_bonus_for_type(o_ptr->bane_type[j]);
                    if (this_bonus > bonus)
                        bonus = this_bonus;
                }
            }
        }
    }

    return bonus;
}

int unique_bane_bonus(monster_type* m_ptr)
{
    int bonus = 0;
    monster_race* r_ptr;

    if (m_ptr == NULL)
        return 0;

    if (p_ptr->entranced)
        return 0;

    if (p_ptr->stun > 100)
        return 0;

    if (!p_ptr->active_ability[S_SPC][SPC_UNIQUE_BANE])
        return 0;

    r_ptr = &r_info[m_ptr->r_idx];

    if (r_ptr->flags1 & RF1_UNIQUE)
    {
        int threshold = 2;
        int uniques_killed = unique_bane_type_killed();

        while (threshold <= uniques_killed)
        {
            threshold *= 2;
            bonus++;
        }
    }

    return bonus;
}

int unique_bane_type_killed(void)
{
    int i;
    int uniques_killed = 0;

    for (i = 1; i < z_info->r_max; i++)
    {
        monster_race* r_ptr = &r_info[i];

        if (!(r_ptr->flags1 & RF1_UNIQUE))
            continue;

        if (r_ptr->max_num == 0)
            uniques_killed++;
    }

    return uniques_killed;
}

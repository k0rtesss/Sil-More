/* File: player/player-abilities.c */

#include "angband.h"

#include "player/player-abilities.h"

int ability_index(int skilltype, int abilitynum)
{
    int i;

    for (i = 0; i < z_info->b_max; i++)
    {
        ability_type* b_ptr = &b_info[i];

        if (!b_ptr->name)
            continue;

        if (b_ptr->skilltype != skilltype)
            continue;

        if (b_ptr->abilitynum == abilitynum)
            return i;
    }

    return 0;
}

int abilities_in_skill(int skilltype)
{
    int i;
    int count = 0;

    for (i = 0; i < z_info->b_max; i++)
    {
        ability_type* b_ptr = &b_info[i];

        if (!b_ptr->name)
            continue;

        if (b_ptr->skilltype != skilltype)
            continue;

        if (p_ptr->innate_ability[skilltype][b_ptr->abilitynum])
            count++;
    }

    return count;
}

static bool prereq_abilities_met(const ability_type* b_ptr)
{
    int i;

    if (b_ptr->prereqs > 0 && !(p_ptr->active_ability[S_PER][PER_QUICK_STUDY]))
    {
        for (i = 0; i < b_ptr->prereqs; i++)
        {
            if (p_ptr->innate_ability[b_ptr->prereq_skilltype[i]]
                                     [b_ptr->prereq_abilitynum[i]])
            {
                return true;
            }
        }

        return false;
    }

    return true;
}

bool ability_prereqs_met(int skilltype, int abilitynum)
{
    ability_type* b_ptr = &b_info[ability_index(skilltype, abilitynum)];

    return prereq_abilities_met(b_ptr);
}

bool prereqs(int skilltype, int abilitynum)
{
    ability_type* b_ptr = &b_info[ability_index(skilltype, abilitynum)];

    if (p_ptr->skill_base[skilltype] < b_ptr->level)
        return false;

    return ability_prereqs_met(skilltype, abilitynum);
}

/* File: encumbrance.c */

#include "angband.h"
#include "player/encumbrance.h"


/*
 * Computes current weight limit in tenths of pounds.
 *
 * 100 + a compounding 20% bonus per point of str
 */
int weight_limit(void)
{
    int i;
    int limit;

    limit = 1000;
    if (p_ptr->stat_use[A_STR] >= 0)
    {
        for (i = 0; i < p_ptr->stat_use[A_STR]; i++)
        {
            limit = limit * 12 / 10;
        }
    }
    else
    {
        for (i = 0; i < -(p_ptr->stat_use[A_STR]); i++)
        {
            limit = limit * 10 / 12;
        }
    }

    /* CUR_WEAK: curse reduces weight limit by 20% per stack; blessing increases by 20% per stack */
    int weak_delta = curse_flag_delta_cur(CUR_WEAK);
    if (weak_delta > 0) {
        for (i = 0; i < weak_delta; i++) limit = limit * 8 / 10;
    } else if (weak_delta < 0) {
        for (i = 0; i < -weak_delta; i++) limit = limit * 12 / 10;
    }

    /* Return the result */
    return (limit);
}

bool sprinting(void)
{
    int i;
    int turns = 1;

    if (p_ptr->active_ability[S_EVN][EVN_SPRINTING])
    {
        for (i = 1; i < 4; i++)
        {
            if ((p_ptr->previous_action[i] >= 1)
                && (p_ptr->previous_action[i] <= 9)
                && (p_ptr->previous_action[i] != 5))
            {
                if ((p_ptr->previous_action[i + 1] >= 1)
                    && (p_ptr->previous_action[i + 1] <= 9)
                    && (p_ptr->previous_action[i + 1] != 5))
                {
                    if (p_ptr->previous_action[i]
                        == p_ptr->previous_action[i + 1])
                    {
                        turns++;
                    }
                    else if (p_ptr->previous_action[i]
                        == cycle[chome[p_ptr->previous_action[i + 1]] - 1])
                    {
                        turns++;
                    }
                    else if (p_ptr->previous_action[i]
                        == cycle[chome[p_ptr->previous_action[i + 1]] + 1])
                    {
                        turns++;
                    }
                }
            }
        }
    }

    return (turns >= 4);
}

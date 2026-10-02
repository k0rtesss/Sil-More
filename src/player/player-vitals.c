#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "metarun.h"
#include "pane.h"
#include "supplies.h"
#include "item_set.h"
#include "player/player-upkeep-internal.h"

/* Preserve the reserve ratio, rounding only the final value to the nearest
 * point. An uninitialized maximum starts with a full reserve. */
static int rescale_vital(int current, int old_max, int new_max)
{
    if (old_max <= 0)
        return new_max;
    s64b scaled = (s64b)current * new_max;
    scaled += scaled < 0 ? -(old_max / 2) : old_max / 2;
    return (int)(scaled / old_max);
}

/*
 * Calculate maximum voice.
 *
 * This function induces status messages.
 */
extern void calc_voice(void)
{
    int msp;
    int i;
    int tmp;

    /* Get voice value */
    // 20 + a compounding 20% bonus per point of gra

    tmp = 20 * 100;

    if (p_ptr->stat_use[A_GRA] >= 0)
    {
        for (i = 0; i < p_ptr->stat_use[A_GRA]; i++)
        {
            tmp = tmp * 12 / 10;
        }
    }
    else
    {
        for (i = 0; i < -(p_ptr->stat_use[A_GRA]); i++)
        {
            tmp = tmp * 10 / 12;
        }
    }
    msp = tmp / 100;

    /* New maximum voice */
    if (p_ptr->msp != msp)
    {
        p_ptr->csp = rescale_vital(p_ptr->csp, p_ptr->msp, msp);

        /* Save new limit */
        p_ptr->msp = msp;

        /* Hack - any change in max voice resets frac */
        p_ptr->csp_frac = 0;

        /* Display sp later */
        p_ptr->redraw |= (PR_VOICE);

        /* Window stuff */
        p_ptr->window |= (PW_PLAYER_0);
    }

    /* Hack -- handle "xtra" mode */
    if (character_xtra)
        return;
}

/*
 * Calculate the player's (maximal) hit points
 *
 * Adjust current hitpoints if necessary
 *
 * Sil - modified substantially to reflect absence of chance and fixed bonus,
 * not per level
 */
void calc_hitpoints(void)
{
    int mhp;
    int i;
    int tmp;

    /* Get hitpoint value */
    // 20 + a compounding 16% bonus per point of con, plus 5 HP flat bonus

    tmp = 20 * 100;
    if (p_ptr->stat_use[A_CON] >= 0)
    {
        for (i = 0; i < p_ptr->stat_use[A_CON]; i++)
        {
            tmp = tmp * 116 / 100;
        }
    }
    else
    {
        for (i = 0; i < -(p_ptr->stat_use[A_CON]); i++)
        {
            tmp = tmp * 100 / 116;
        }
    }
    mhp = tmp / 100 + 5;

    /* New maximum hitpoints */
    if (p_ptr->mhp != mhp)
    {
        p_ptr->chp = rescale_vital(p_ptr->chp, p_ptr->mhp, mhp);

        /* Save new limit */
        p_ptr->mhp = mhp;

        /* Hack - any change in max hitpoint resets frac */
        p_ptr->chp_frac = 0;

        /* Display hp later */
        p_ptr->redraw |= (PR_HP);

        /* Window stuff */
        p_ptr->window |= (PW_PLAYER_0);
    }
}

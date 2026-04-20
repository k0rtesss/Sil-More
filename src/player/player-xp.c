/* File: player-xp.c */
/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (C) 2025-2026 Sil-More contributors
 *
 * This file is part of Sil-More.
 *
 * Sil-More is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Sil-More is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See LICENSE.md
 * for more details.
 */

/*
 * Player experience and falling-damage helpers.
 * Split mechanically from xtra2.c during the WP13 refactor port.
 */

#include "angband.h"
#include "player/killer.h"

/*
 * Falling damage. 3d4 for one floor, 6d4 for two floors.
 */
void falling_damage(bool stun)
{
    int dice = 0;
    int dam;

    cptr message;

    if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_CHASM)
    {
        if (p_ptr->depth >= MORGOTH_DEPTH - 1)
            dice = 3; // as this means you will only fall one floor
        else
            dice = 6;
        message = "falling down a chasm";
    }
    else if (cave_stair_bold(p_ptr->py, p_ptr->px))
    {
        dice = 3;
        message = "a collapsing stair";
    }
    else
    {
        dice = 3;
        message = "a collapsing floor";
    }

    // calculate the damage
    dam = damroll(dice, 4);

    if (dice > 0)
    {
        // update the combat rolls window
        update_combat_rolls1b(NULL, PLAYER, true);
        update_combat_rolls2(dice, 4, dam, -1, -1, 0, 0, GF_HURT, false);

        /* Take the damage */
        killer_mark_other(SCORE_KILLER_FALL);
        take_hit(dam, message);
    }

    if (stun && allow_player_stun(NULL))
    {
        set_stun(p_ptr->stun + dam * 5);
    }

    // reset staircasiness
    p_ptr->staircasiness = 0;
}

/*
 * Advance experience levels and print experience
 */
void check_experience(void)
{
    /* Hack -- lower limit */
    if (p_ptr->exp < 0)
        p_ptr->exp = 0;

    /* Hack -- lower limit */
    if (p_ptr->new_exp < 0)
        p_ptr->new_exp = 0;

    /* Hack -- upper limit */
    if (p_ptr->exp > PY_MAX_EXP)
        p_ptr->exp = PY_MAX_EXP;

    /* Hack -- upper limit */
    if (p_ptr->new_exp > PY_MAX_EXP)
        p_ptr->new_exp = PY_MAX_EXP;

    /* Hack -- maintain "max" experience */
    if (p_ptr->new_exp > p_ptr->exp)
        p_ptr->new_exp = p_ptr->exp;

    /* Redraw experience */
    p_ptr->redraw |= (PR_EXP);

    /* Redraw stuff */
    redraw_stuff();
}

/*
 * Gain experience
 */
void gain_exp(s32b amount)
{
    if (birth_fixed_exp)
    {
        return;
    }

    /* Gain some experience */
    p_ptr->exp += amount;
    p_ptr->new_exp += amount;

    /* Check Experience */
    check_experience();
}

/*
 * Lose experience
 */
void lose_exp(s32b amount)
{
    /* Never drop below zero experience */
    if (amount > p_ptr->new_exp)
        amount = p_ptr->new_exp;

    /* Lose some experience */
    p_ptr->new_exp -= amount;
    p_ptr->exp -= amount;

    /* Check Experience */
    check_experience();
}

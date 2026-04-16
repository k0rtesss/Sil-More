/* File: cmd-combat-rolls.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "object/object-ui-select.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"

static int overwhelming_att_mod(monster_type* m_ptr);

int skill_check(
    monster_type* m_ptr1, int skill, int difficulty, monster_type* m_ptr2)
{
    int skill_total;
    int difficulty_total;
    int skill_total_alt;
    int difficulty_total_alt;

    // bonuses against your enemy of choice
    if ((m_ptr1 == PLAYER) && (m_ptr2 != NULL))
        skill += bane_bonus(m_ptr2);
    if ((m_ptr2 == PLAYER) && (m_ptr1 != NULL))
        difficulty += bane_bonus(m_ptr1);

    // elf-bane bonus against you
    if ((m_ptr1 == PLAYER) && (m_ptr2 != NULL))
        difficulty += elf_bane_bonus(m_ptr2);
    if ((m_ptr2 == PLAYER) && (m_ptr1 != NULL))
        skill += elf_bane_bonus(m_ptr1);

    // the basic rolls
    skill_total = dieroll(10) + skill;
    difficulty_total = dieroll(10) + difficulty;

    // alternate rolls for dealing with the curse
    skill_total_alt = dieroll(10) + skill;
    difficulty_total_alt = dieroll(10) + difficulty;

    // player curse?
    if (p_ptr->cursed)
    {
        if (m_ptr1 == PLAYER)
            skill_total = MIN(skill_total, skill_total_alt);
        if (m_ptr2 == PLAYER)
            difficulty_total = MIN(difficulty_total, difficulty_total_alt);
    }

    /* Debugging message */
    if (cheat_skill_rolls)
    {
        msg_format("{%d+%d v %d+%d = %d}.", skill_total - skill, skill,
            difficulty_total - difficulty, difficulty,
            skill_total - difficulty_total);
    }

    return (skill_total - difficulty_total);
}

/*
 * Light hating monsters get a penalty to hit/evn if the player's
 * square is too bright.
 */

static int light_penalty(const monster_type* m_ptr)
{
    int penalty = 0;
    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    if (r_ptr->flags3 & (RF3_HURT_LITE))
    {
        penalty = (cave_light[m_ptr->fy][m_ptr->fx] - 2);

        if (penalty < 0)
            penalty = 0;
    }

    return (penalty);
}

/*
 * Determine the result of an attempt to hit an opponent.
 * Results <= 0 count as misses.
 * Results > 0 are hits and, if high enough, are criticals.
 *
 * The monster is the creature doing the attacking.
 * This is used in displaying the attack roll details.
 * attacker_vis is whether the attacker is visible.
 * this is used in displaying the attack roll details.
 */
int hit_roll(int att, int evn, const monster_type* m_ptr1,
    const monster_type* m_ptr2, bool display_roll)
{
    int attack_score, attack_score_alt;
    int evasion_score, evasion_score_alt;
    bool non_player_visible;

    // determine the visibility for  the combat roll window
    if (m_ptr1 == PLAYER)
    {
        if (m_ptr2 == NULL)
            non_player_visible = true;
        else
            non_player_visible = m_ptr2->ml;
    }
    else
    {
        if (m_ptr1 == NULL)
            non_player_visible = true;
        else
            non_player_visible = m_ptr1->ml;
    }

    // roll the dice...
    attack_score = dieroll(20) + att;
    attack_score_alt = dieroll(20) + att;
    evasion_score = dieroll(20) + evn;
    evasion_score_alt = dieroll(20) + evn;

    // take the worst of two rolls for cursed players
    if (p_ptr->cursed)
    {
        if (m_ptr1 == PLAYER)
        {
            attack_score = MIN(attack_score, attack_score_alt);
        }
        else
        {
            evasion_score = MIN(evasion_score, evasion_score_alt);
        }
    }

    // set the information for the combat roll window
    if (display_roll)
    {
        update_combat_rolls1(m_ptr1, m_ptr2, non_player_visible, att,
            attack_score - att, evn, evasion_score - evn);
    }

    return (attack_score - evasion_score);
}

/*
 * Determines the player's evasion based on all the relevant attributes and
 * modifiers.
 */

int total_player_attack(monster_type* m_ptr, int base)
{
    int att = base;

    // reward concentration ability (if applicable)
    att += concentration_bonus(m_ptr->fy, m_ptr->fx);

    // reward focused attack ability (if applicable)
    att += focused_attack_bonus();

    // reward bane ability (if applicable)
    att += bane_bonus(m_ptr);

    // reward artifact-granted bane (if applicable)
    att += artifact_bane_bonus(m_ptr);

    // reward unique bane ability (if applicable)
    att += unique_bane_bonus(m_ptr);

    // reward master hunter ability (if applicable)
    att += master_hunter_bonus(m_ptr);

    // penalise distance -- note that this penalty will equal 0 in melee
    att -= distance(p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx) / 5;

    // halve attack score for certain situations (and only halve positive
    // scores!)
    if (att > 0)
    {
        // penalise the player if (s)he can't see the monster
        if (!m_ptr->ml)
            att /= 2;

        // penalise the player if (s)he is in a pit or web
        if (cave_pit_bold(p_ptr->py, p_ptr->px)
            || (cave_feat[p_ptr->py][p_ptr->px] == FEAT_TRAP_WEB))
        {
            att /= 2;
        }
    }

    return (att);
}

/*
 * Determines the player's evasion based on all the relevant attributes and
 * modifiers.
 */

int total_player_evasion(monster_type* m_ptr, bool archery)
{
    int evn = p_ptr->skill_use[S_EVN];

    // reward successful use of the dodging ability
    evn += dodging_bonus();

    // reward successful use of the bane ability
    evn += bane_bonus(m_ptr);

    // reward artifact-granted bane (if applicable)
    evn += artifact_bane_bonus(m_ptr);

    // reward unique bane ability (if applicable)
    evn += unique_bane_bonus(m_ptr);

    // halve evasion for certain situations (and only halve positive evasion!)
    if (evn > 0)
    {
        // penalise the player if (s)he can't see the monster
        if (!m_ptr->ml)
            evn /= 2;

        // penalise targets of archery attacks
        if (archery)
            evn /= 2;

        // penalise the player if (s)he is in a pit or web
        if (cave_pit_bold(p_ptr->py, p_ptr->px)
            || (cave_feat[p_ptr->py][p_ptr->px] == FEAT_TRAP_WEB))
        {
            evn /= 2;
        }
    }

    return (evn);
}

/*
 * Determines a monster's attack score based on all the relevant attributes and
 * modifiers.
 */

int total_monster_attack(monster_type* m_ptr, int base)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    int att = base;
    bool unseen = false;

    // penalise stunning
    if (m_ptr->stunned)
        att -= 2;

    // penalise being in bright light for light-averse monsters
    att -= light_penalty(m_ptr);

    // reward surrounding the player
    att += overwhelming_att_mod(m_ptr);

    // penalise distance
    att -= distance(p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx) / 5;

    // elf-bane bonus
    att += elf_bane_bonus(m_ptr);

    // unique bane penalty (player ability affecting monster)
    att -= unique_bane_bonus(m_ptr);

    // halve attack score for certain situations (and only halve positive
    // scores!)
    if (att > 0)
    {
        // check if player is unseen
        if ((r_ptr->light > 0) && strchr("@G", r_ptr->d_char)
            && (cave_light[p_ptr->py][p_ptr->px] <= 0))
            unseen = true;

        // penalise monsters who can't see the player
        if (unseen)
            att /= 2;
    }

    return (att);
}

/*
 * Determines a monster's evasion based on all the relevant attributes and
 * modifiers.
 */

int total_monster_evasion(monster_type* m_ptr, bool archery)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    int evn = r_ptr->evn;
    evn -= m_ptr->song_evasion_penalty;
    bool unseen = false;

    // penalise stunning
    if (m_ptr->stunned)
        evn -= 2;

    // penalise being in bright light for light-averse monsters
    evn -= light_penalty(m_ptr);

    // elf-bane bonus
    evn += elf_bane_bonus(m_ptr);

    // unique bane penalty (player ability affecting monster)
    evn -= unique_bane_bonus(m_ptr);

    // halve evasion for certain situations (and only halve positive evasion!)
    if (evn > 0)
    {
        // check if player is unseen
        if ((r_ptr->light > 0) && strchr("@G", r_ptr->d_char)
            && (cave_light[p_ptr->py][p_ptr->px] <= 0))
            unseen = true;

        // penalise unwary monsters, or those who can't see the player
        if (unseen || (m_ptr->alertness < ALERTNESS_ALERT))
            evn /= 2;

        // penalise targets of archery attacks
        if (archery)
            evn /= 2;
    }

    // finally, all sleeping monsters have -5 total evasion
    if (m_ptr->alertness < ALERTNESS_UNWARY)
        evn = -5;

    return (evn);
}

/*
 * Monsters are already given a large set penalty for being asleep
 * (total evasion mod of -5) and unwary (evasion score / 2),
 * but we also give a bonus for high stealth characters who have ASSASSINATION.
 */

int stealth_melee_bonus(const monster_type* m_ptr, bool allow_unseen)
{
    int stealth_bonus = 0;

    if (p_ptr->active_ability[S_STL][STL_ASSASSINATION])
    {
        bool visible_target = allow_unseen || m_ptr->ml;
        bool unaware_target = (m_ptr->alertness < ALERTNESS_ALERT)
            || song_disguise_monster_is_fooled(m_ptr);

        if (unaware_target && visible_target && !(p_ptr->confused))
        {
            stealth_bonus = p_ptr->skill_use[S_STL];
        }
    }
    return (stealth_bonus);
}

/*
 * Give a bonus to attack the player depending on the number of adjacent
 * monsters. This is +1 for monsters near the attacker or to the sides, and +2
 * for monsters in the three positions behind the player:
 *
 * 1M1  M11
 * 1@1  1@2
 * 222  122
 *
 * We should lessen this with the crowd fighting ability
 */
static int overwhelming_att_mod(monster_type* m_ptr)
{
    int mod = 0;
    int dir;
    int dy, dx;
    int py = p_ptr->py;
    int px = p_ptr->px;

    // determine the main direction from the player to the monster
    dir = rough_direction(py, px, m_ptr->fy, m_ptr->fx);

    // extract the deltas from the direction
    dy = ddy[dir];
    dx = ddx[dir];

    // if monster in an orthogonal direction   753
    //                                         8@M
    //                                         642
    if (dy * dx == 0)
    {
        // increase modifier for monsters engaged with the player...
        if (attacker_at(py + dx + dy, px - dy + dx))
            mod++; // direction 2
        if (attacker_at(py - dx + dy, px + dy + dx))
            mod++; // direction 3
        if (attacker_at(py + dx, px - dy))
            mod++; // direction 4
        if (attacker_at(py - dx, px + dy))
            mod++; // direction 5

        // ...especially if they are behind the player
        if (attacker_at(py + dx - dy, px - dy - dx))
            mod += 2; // direction 6
        if (attacker_at(py - dx - dy, px + dy - dx))
            mod += 2; // direction 7
        if (attacker_at(py - dy, px - dx))
            mod += 2; // direction 8
    }
    // if monster in a diagonal direction   875
    //                                      6@3
    //                                      42M
    else
    {
        // increase modifier for monsters engaged with the player...
        if (attacker_at(py + dy, px))
            mod++; // direction 2
        if (attacker_at(py, px + dx))
            mod++; // direction 3
        if (attacker_at(py + dx, px - dy))
            mod++; // direction 4
        if (attacker_at(py - dx, px + dy))
            mod++; // direction 5

        // ...especially if they are behind the player
        if (attacker_at(py - dy, px))
            mod += 2; // direction 6
        if (attacker_at(py, px - dx))
            mod += 2; // direction 7
        if (attacker_at(py - dy, px - dx))
            mod += 2; // direction 8
    }

    // adjust for crowd fighting ability
    if (p_ptr->active_ability[S_EVN][EVN_CROWD_FIGHTING])
    {
        mod /= 2;
    }

    return (mod);
}

/*
 * Determines the number of bonus dice from a (potentially) critical hit
 *
 * bonus of 1 die for every (6 + weight_in_pounds) over what is needed.
 * (using rounding at 0.5 instead of always rounding up)
 *
 * Thus for a Dagger (0.8lb):         7, 14, 20, 27...  (6+weight)
 *            Short Sword (1.5lb):    8, 15, 23, 30...
 *            Long Sword (3lb):       9, 18, 27, 35...
 *            Bastard Sword (4lb):   10, 20, 30, 40...
 *            Great Sword (7lb):     13, 26, 39, 52...
 *            Shortbow (2lb):         8, 16, 24, 32...
 *            Longbow (3lb):          9, 18, 27, 36...
 *            m 1dX (2lb):            8, 16, 24, 32...
 *            m 2dX (4lb):           10, 20, 30, 40...
 *            m 3dX (6lb):           12, 24, 36, 48...
 *
 * (old versions)
 * Thus for a Dagger (0.8lb):         9, 13, 17, 21...  5 then (3+weight)
 *            Short Sword (1.5lb):   10, 14, 19, 23...
 *            Long Sword (3lb):      11, 17, 23, 29...
 *            Bastard Sword (4lb):   12, 19, 26, 33...
 *            Great Sword (7lb):     15, 25, 35, 45...
 *            Shortbow (2lb):        10, 15, 20, 25...
 *            Longbow (3lb):         11, 17, 23, 29...
 *            m 1dX (2lb):           10, 15, 20, 25...
 *            m 2dX (4lb):           12, 19, 26, 33...
 *            m 3dX (6lb):           14, 23, 32, 41...
 * Thus for a Dagger (0.8lb):        11, 12, 13, 14...  (10 then weightx)
 *            Short Sword (1.5lb):   12, 13, 15, 16...
 *            Long Sword (3lb):      13, 16, 19, 22...
 *            Bastard Sword (4lb):   14, 18, 22, 26...
 *            Great Sword (7lb):     17, 24, 31, 38...
 *            Shortbow (2lb):        12, 14, 16, 18...
 *            Longbow (3lb):         13, 16, 19, 22...
 * Thus for a Dagger (0.8lb):         6, 12, 18, 24...  (5+weight)
 *            Short Sword (1.5lb):    7, 13, 20, 26...
 *            Long Sword (3lb):       8, 16, 24, 32...
 *            Bastard Sword (4lb):    9, 18, 27, 36...
 *            Great Sword (7lb):     12, 24, 36, 48...
 *            Shortbow (2lb):         7, 14, 21, 28...
 *            Longbow (3lb):          8, 16, 24, 32...
 * Thus for a Dagger (0.8lb):         4,  8, 12, 16...  (3+weight)
 *            Short Sword (1.5lb):    5,  9, 14, 18...
 *            Long Sword (3lb):       6, 12, 18, 25...
 *            Bastard Sword (4lb):    7, 14, 21, 28...
 *            Great Sword (7lb):     10, 20, 30, 40...
 *            Shortbow (2lb):         5, 10, 15, 20...
 *            Longbow (3lb):          6, 12, 18, 24...
 * Thus for a Dagger (0.8lb):         8, 12, 15, 18...  (old1)
 *            Short Sword (1.5lb):    9, 14, 18, 23...
 *            Long Sword (3lb):      11, 17, 23, 29...
 *            Bastard Sword (3.5lb): 11, 18, 24, 31...
 *            Great Sword (7lb):     15, 25, 35, 45...
 * Thus for a Dagger (0.8lb):         7, 10, 12, 14...  (old2)
 *            Short Sword (1.5lb):    8, 12, 15, 19...
 *            Long Sword (3lb):      10, 15, 20, 25...
 *            Bastard Sword (3.5lb): 10, 16, 21, 27...
 *            Great Sword (7lb):     14, 23, 32, 41...
 */
int crit_bonus(int hit_result, int weight, const monster_race* r_ptr,
    int skill_type, bool thrown, monster_type* attacker, const object_type* o_ptr)
{
    monster_type* m_ptr = attacker;
    int crit_bonus_dice;
    int crit_seperation = 70;

    if (attacker != NULL && attacker != PLAYER)
    {
        int shift = curse_flag_delta_cur(CUR_CRIT_THRESH_SHIFT);
        if (shift) hit_result += shift;
    }

    // When attacking a monster...
    if (r_ptr->level != 0)
    {
        // Can have improved criticals for melee
        if ((skill_type == S_MEL) && p_ptr->active_ability[S_MEL][MEL_FINESSE])
            crit_seperation -= 20;

        if ((skill_type == S_MEL) && thrown && o_ptr
            && (p_ptr->active_ability[S_MEL][MEL_THROWING]
                || object_grants_ability(o_ptr, S_MEL, MEL_THROWING))
            && player_can_treat_as_throwing(o_ptr))
        {
            crit_seperation -= 10;
        }

        // Can have improved criticals for melee with one handed weapons
        // Special case: Maedhros character can use Subtlety with hand-and-a-half weapons
        bool maedhros_hand_and_half = (c_info[p_ptr->pcharacter].flags_u & UNQ_MEL_MAEDHROS)
            && (k_info[(&inventory[INVEN_WIELD])->k_idx].flags3 & (TR3_HAND_AND_A_HALF))
            && (!inventory[INVEN_ARM].k_idx);
        
        if ((skill_type == S_MEL) && p_ptr->active_ability[S_MEL][MEL_CONTROL]
            && !thrown && (!two_handed_melee() || maedhros_hand_and_half) && !inventory[INVEN_ARM].k_idx)
            crit_seperation -= 20;

        // Subtlety can work with throwing if the weapon has TR4_SUBTLETY_THROW flag.
        // The flag extends an existing Subtlety ability; it does not grant one.
        if ((skill_type == S_MEL) && thrown && o_ptr
            && p_ptr->active_ability[S_MEL][MEL_CONTROL])
        {
            u32b st_f1, st_f2, st_f3, st_f4;
            object_flags4(o_ptr, &st_f1, &st_f2, &st_f3, &st_f4);
            if (st_f4 & TR4_SUBTLETY_THROW)
                crit_seperation -= 20;
        }

        // Can have inferior criticals for melee
        if ((skill_type == S_MEL) && p_ptr->active_ability[S_MEL][MEL_POWER])
            crit_seperation += 10;
    }

    // note: the +4 in this calculation is for rounding purposes
    crit_bonus_dice = (hit_result * 10 + 4) / (crit_seperation + weight);

    // When attacking a monster...
    if (r_ptr->level != 0)
    {
        // Resistance to criticals doubles what you need for each bonus die
        if (r_ptr->flags1 & (RF1_RES_CRIT))
            crit_bonus_dice /= 2;

        // certain creatures cannot suffer crits as they have no vulnerable
        // areas
        if (r_ptr->flags1 & (RF1_NO_CRIT))
            crit_bonus_dice = 0;
    }
    else if (m_ptr && p_ptr->active_ability[S_PER][PER_OUTWIT]
        && skill_check(PLAYER, p_ptr->skill_use[S_PER],
               monster_skill(m_ptr, S_PER), m_ptr)
            > 0)
    {
        crit_bonus_dice = 0;
    }

    // can't have fewer than zero dice
    if (crit_bonus_dice < 0)
        crit_bonus_dice = 0;

    return crit_bonus_dice;
}

/*
 * Describes the effect of a slay
 */

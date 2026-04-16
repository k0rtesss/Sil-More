/* File: spell/spell-projection.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

/*
 * Core spell projection engine: bolts, beams, balls, arcs, and area effects.
 * Split from spells1.c and spells2.c for better code organization.
 */

#include "angband.h"
#include "app/app-session.h"
#include "externs.h"
#include "log/log.h"
#include "platform-frame.h"
#include "player/killer.h"
#include "spell/spell-projection.h"
#include "spell/spell-damage.h"

static int death_count;
/*
 * Mega-Hack -- track "affected" monsters (see "project()" comments)
 */
static int project_m_n;
static int project_m_x;
static int project_m_y;

/*
 * Magically close/lock/restore a door at a particular grid
 */
bool lock_door(int y, int x, int power)
{
    int lock_level;
    int obvious = false;

    // ignore warded doors
    if (cave_glyph(y, x))
        return false;

    if (cave_feat[y][x] == FEAT_BROKEN)
        power -= 10;

    if ((power > 0) && (cave_m_idx[y][x] == 0))
    {
        if (cave_known_closed_door_bold(y, x) || (cave_feat[y][x] == FEAT_OPEN)
            || (cave_feat[y][x] == FEAT_BROKEN))
        {
            if ((cave_feat[y][x] == FEAT_OPEN)
                || (cave_feat[y][x] == FEAT_BROKEN))
            {
                cave_set_feat(y, x, FEAT_DOOR_HEAD);

                obvious = true;

                if (cave_info[y][x] & (CAVE_SEEN))
                {
                    msg_print("The door slams shut.");
                }
                else
                {
                    msg_print("You hear a door slam shut.");
                }
            }

            // lock the door more firmly than it was before
            lock_level = cave_feat[y][x] - FEAT_DOOR_HEAD + power / 2;
            if (lock_level > 7)
            {
                lock_level = 7;
            }

            if (cave_feat[y][x] != FEAT_DOOR_HEAD + lock_level)
            {
                cave_set_feat(y, x, FEAT_DOOR_HEAD + lock_level);

                msg_print("You hear a 'click'.");
            }

            /* Update the flow code and visuals */
            p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
        }
    }

    return (obvious);
}

bool lock_doors_radius(int y0, int x0, int radius, int power)
{
    bool obvious = false;

    if (radius < 0)
        return false;

    for (int y = y0 - radius; y <= y0 + radius; y++)
    {
        for (int x = x0 - radius; x <= x0 + radius; x++)
        {
            if (!in_bounds_fully(y, x))
                continue;

            if (distance(y0, x0, y, x) > radius)
                continue;

            if (lock_door(y, x, power))
                obvious = true;
        }
    }

    return obvious;
}

/*
 * We are called from "project()" to "damage" terrain features
 *
 * We are called both for "beam" effects and "ball" effects.
 *
 * The "r" parameter is the "distance from ground zero".
 *
 * Note that we determine if the player can "see" anything that happens
 * by taking into account: blindness, line-of-sight, and illumination.
 *
 * We return "true" if the effect of the projection is "obvious".
 *
 * Hack -- We also "see" grids which are "memorized".
 *
 * Perhaps we should affect doors and/or walls.
 */
static bool project_f(
    int who, int y, int x, int dist, int dd, int ds, int dif, int typ)
{
    bool obvious = false;
    monster_type* who_ptr = (who == -1) ? PLAYER : &mon_list[who]; // Sil-y

    /* Unused parameters */
    (void)dist;
    (void)dd;
    (void)ds;

    /* Analyze the type */
    switch (typ)
    {
    /* Ignore most effects */

    /* Destroy Traps */
    case GF_KILL_TRAP:
    {
        /* Destroy traps */
        if (cave_trap_bold(y, x))
        {
            /* Check line of sight */
            if (player_has_los_bold(y, x) && !cave_floorlike_bold(y, x))
            {
                obvious = true;
            }

            /* Forget the trap */
            cave_info[y][x] &= ~(CAVE_MARK);

            /* Destroy the trap */
            cave_set_feat(y, x, FEAT_FLOOR);
        }

        break;
    }

    /* unlock/open/break Doors */
    case GF_KILL_DOOR:
    {
        if (cave_known_closed_door_bold(y, x) && !cave_glyph(y, x))
        {
            int result = skill_check(who_ptr, dif, 0, NULL);

            if (result <= 0)
            {
                /* Do nothing */
            }
            else if (result <= 5)
            {
                /* Unlock the door */
                cave_set_feat(y, x, FEAT_DOOR_HEAD + 0x00);

                msg_print("You hear a 'click'.");
            }
            else if (result <= 10)
            {
                /* Forget the door */
                // cave_info[y][x] &= ~(CAVE_MARK);

                /* Open the door */
                cave_set_feat(y, x, FEAT_OPEN);

                /* Update the flow code */
                p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

                obvious = true;

                if (cave_info[y][x] & (CAVE_SEEN))
                {
                    msg_print("The door flies open.");
                }
                else
                {
                    msg_print("You hear a door burst open.");
                }
            }
            else
            {
                /* Break the door */
                cave_set_feat(y, x, FEAT_BROKEN);

                /* Update the flow code */
                p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

                obvious = true;

                if (cave_info[y][x] & (CAVE_SEEN))
                {
                    msg_print("The door is ripped from its hinges.");
                }
                else
                {
                    msg_print("You hear a door burst open.");
                }
            }
        }

        if (cave_feat[y][x] == FEAT_RUBBLE)
        {
            int result = skill_check(who_ptr, dif, 0, NULL);

            if (result <= 0)
            {
                /* Do nothing */
            }

            else
            {
                /* Disperse the rubble */
                cave_set_feat(y, x, FEAT_FLOOR);

                obvious = true;

                /* Update the flow code */
                p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

                if (cave_info[y][x] & (CAVE_SEEN))
                {
                    msg_print("The rubble is scattered across the floor.");
                }
                else
                {
                    msg_print("You hear a loud rumbling.");
                }
            }
        }

        break;
    }

    /* Destroy walls (and doors) */
    case GF_KILL_WALL:
    {
        /* Non-walls (etc) */
        if (cave_floor_bold(y, x))
            break;

        /* Permanent walls */
        if (cave_feat[y][x] == FEAT_WALL_PERM)
            break;

        /* Granite */
        if (cave_feat[y][x] >= FEAT_WALL_EXTRA
            && skill_check(PLAYER, dif, 14, NULL) > 0)
        {
            /* Message */
            if (cave_info[y][x] & (CAVE_MARK))
            {
                msg_print("The wall shatters!");
                obvious = true;
            }

            /* Forget the wall */
            cave_info[y][x] &= ~(CAVE_MARK);

            /* Destroy the wall */
            cave_set_feat(y, x, FEAT_RUBBLE);
        }
        /* Quartz */
        else if (cave_feat[y][x] >= FEAT_QUARTZ
            && skill_check(PLAYER, dif, 12, NULL) > 0)
        {
            /* Message */
            if (cave_info[y][x] & (CAVE_MARK))
            {
                msg_print("The vein shatters!");
                obvious = true;
            }

            /* Forget the wall */
            cave_info[y][x] &= ~(CAVE_MARK);

            /* Destroy the wall */
            cave_set_feat(y, x, FEAT_RUBBLE);
        }
        /* Rubble */
        else if (cave_feat[y][x] == FEAT_RUBBLE
            && skill_check(PLAYER, dif, 10, NULL) > 0)
        {
            /* Message */
            if (cave_info[y][x] & (CAVE_MARK))
            {
                msg_print("The rubble is blown away!");
                obvious = true;
            }

            /* Forget the wall */
            cave_info[y][x] &= ~(CAVE_MARK);

            /* Destroy the rubble */
            cave_set_feat(y, x, FEAT_FLOOR);
        }

        /* Destroy doors (and secret doors) */
        else if (cave_any_closed_door_bold(y, x)
            && skill_check(PLAYER, dif, 8, NULL) > 0)
        {
            /* Hack -- special message */
            if (cave_info[y][x] & (CAVE_MARK))
            {
                msg_print("The door is blown from its hinges!");
                obvious = true;
            }

            /* Forget the wall */
            cave_info[y][x] &= ~(CAVE_MARK);

            /* Destroy the feature */
            cave_set_feat(y, x, FEAT_BROKEN);
        }

        /* Update the visuals */
        p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

        break;
    }

    /* Lock Doors */
    case GF_LOCK_DOOR:
    {
        obvious = lock_door(y, x, skill_check(who_ptr, dif, 0, NULL));

        break;
    }

    /* Lite up the grid */
    case GF_LIGHT:
    {
        // Must make sure it is viewable (passwall was only used to guarantee
        // wall lighting)
        if (cave_info[y][x] & (CAVE_VIEW))
        {
            /* Turn on the light */
            cave_info[y][x] |= (CAVE_GLOW);
        }

        /* Grid is in line of sight */
        if (player_has_los_bold(y, x))
        {
            if (!p_ptr->blind)
            {
                /* Observe */
                obvious = true;
            }

            /* Fully update the visuals */
            p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);
        }

        break;
    }

    /* Darken the grid */
    case GF_DARK_WEAK:
    case GF_DARK:
    {
        if (cave_info[y][x] & (CAVE_GLOW))
        {
            /* Turn off the light */
            cave_info[y][x] &= ~(CAVE_GLOW);

            /* Hack -- Forget "boring" grids */
            if (cave_floorlike_bold(y, x))
            {
                /* Forget */
                cave_info[y][x] &= ~(CAVE_MARK);
            }
            /* Grid is in line of sight */
            if (player_has_los_bold(y, x))
            {
                /* Observe */
                obvious = true;

                /* Fully update the visuals */
                p_ptr->update
                    |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);
            }
        }

        /* All done */
        break;
    }
    }

    /* Return "Anything seen?" */
    return (obvious);
}

/*
 * We are called from "project()" to "damage" objects
 *
 * We are called both for "beam" effects and "ball" effects.
 *
 * Perhaps we should only SOMETIMES damage things on the ground.
 *
 * The "r" parameter is the "distance from ground zero".
 *
 * Note that we determine if the player can "see" anything that happens
 * by taking into account: blindness, line-of-sight, and illumination.
 *
 * Hack -- We also "see" objects which are "memorized".
 *
 * We return "true" if the effect of the projection is "obvious".
 */
static bool project_o(int who, int y, int x, int dd, int ds, int dif, int typ)
{
    s16b this_o_idx, next_o_idx = 0;

    bool obvious = false;

    u32b f1, f2, f3;

    char o_name[80];

    /* Unused parameters */
    (void)who;
    (void)dif;

    /* Scan all objects in the grid */
    for (this_o_idx = cave_o_idx[y][x]; this_o_idx; this_o_idx = next_o_idx)
    {
        object_type* o_ptr;

        bool is_art = false;
        bool ignore = false;
        bool plural = false;
        bool do_kill = false;

        cptr note_kill = NULL;

        // Sil-y: previously used damage to see if items were broken, now just
        // ignoring damage
        // int dam = damroll(dd, ds);
        (void)dd; // cast to soothe compiler warnings
        (void)ds; // cast to soothe compiler warnings

        /* Get the object */
        o_ptr = &o_list[this_o_idx];

        /* Get the next object */
        next_o_idx = o_ptr->next_o_idx;

        /* Extract the flags */
        object_flags(o_ptr, &f1, &f2, &f3);

        /* Get the "plural"-ness */
        if (o_ptr->number > 1)
            plural = true;

        /* Check for artefact */
        if (artefact_p(o_ptr))
            is_art = true;

        /* Analyze the type */
        switch (typ)
        {
        /* Acid -- Lots of things */
        case GF_ACID:
        {
            if (hates_acid(o_ptr))
            {
                do_kill = true;
                note_kill = (plural ? " melt!" : " melts!");
                if (f3 & (TR3_IGNORE_ACID))
                    ignore = true;
            }
            break;
        }

        /* Elec -- Rings */
        case GF_ELEC:
        {
            if (hates_elec(o_ptr))
            {
                do_kill = true;
                note_kill = (plural ? " are destroyed!" : " is destroyed!");
                if (f3 & (TR3_IGNORE_ELEC))
                    ignore = true;
            }
            break;
        }

        /* Fire -- Flammable objects */
        case GF_FIRE:
        {
            if (hates_fire(o_ptr))
            {
                do_kill = true;
                note_kill = (plural ? " burn up!" : " burns up!");
                if (f3 & (TR3_IGNORE_FIRE))
                    ignore = true;
            }
            break;
        }

        /* Cold -- potions and flasks */
        case GF_COLD:
        {
            if (hates_cold(o_ptr))
            {
                note_kill = (plural ? " shatter!" : " shatters!");
                do_kill = true;
                if (f3 & (TR3_IGNORE_COLD))
                    ignore = true;
            }
            break;
        }

        /* Hack -- break potions and such */
        case GF_SOUND:
        case GF_EARTHQUAKE:
        {
            if (hates_cold(o_ptr))
            {
                note_kill = (plural ? " shatter!" : " shatters!");
                do_kill = true;
            }
            break;
        }

        /* Unlock chests */
        case GF_KILL_TRAP:
        case GF_KILL_DOOR:
        {
            /* Chests are noticed only if trapped or locked */
            if (o_ptr->tval == TV_CHEST)
            {
                /* Disarm/Unlock traps */
                if (o_ptr->pval > 0)
                {
                    /* Disarm or Unlock */
                    o_ptr->pval = (0 - o_ptr->pval);

                    /* Identify */
                    object_known(o_ptr);
                }
            }

            break;
        }

        /* Mass-identify */
        case GF_IDENTIFY:
        {
            /* Ignore hidden objects */
            if (!o_ptr->marked)
                continue;

            /* Ignore known objects */
            if (object_known_p(o_ptr))
                continue;

            /* Identify object */
            /* Note the first argument */
            (void)do_ident_item(-1, o_ptr);

            /* Redraw purple dots */
            dungeon_mark_map_for_redraw();

            break;
        }
        }

        /* Attempt to destroy the object */
        if (do_kill)
        {
            /* Effect "observed" */
            if (o_ptr->marked)
            {
                obvious = true;
                object_desc(o_name, sizeof(o_name), o_ptr, false, 0);
            }

            /* Artefacts, and other objects, get to resist */
            if (is_art || ignore)
            {
                /* Observe the resist */
                if (o_ptr->marked)
                {
                    msg_format("The %s %s unaffected!", o_name,
                        (plural ? "are" : "is"));
                }
            }

            /* Kill it */
            else
            {
                /* Describe if needed */
                if (o_ptr->marked && note_kill)
                {
                    msg_format("The %s%s", o_name, note_kill);
                }

                if ((o_ptr->tval == TV_CHEST) && (typ != GF_SOUND)
                    && (typ != GF_EARTHQUAKE))
                {
                    chest_release_contents(o_ptr, y, x, typ);
                }

                /* Delete the object */
                delete_object_idx(this_o_idx);

                /* Redraw */
                dungeon_mark_map_for_redraw();
            }
        }
    }

    /* Return "Anything seen?" */
    return (obvious);
}

/*
 * Helper function for "project()" below.
 *
 * Handle a beam/bolt/ball/arc causing damage to a monster.
 *
 * This routine takes a "source monster" (by index) which is mostly used to
 * determine if the player is causing the damage, and a "radius" (see below),
 * which is used to decrease the power of explosions with distance, and a
 * location, via integers which are modified by certain types of attacks
 * (polymorph and teleport being the obvious ones), a default damage, which
 * is modified as needed based on various properties, and finally a "damage
 * type" (see below).
 *
 * Note that this routine can handle "no damage" attacks (like teleport) by
 * taking a "zero" damage, and can even take "parameters" to attacks (like
 * confuse) by accepting a "damage", using it to calculate the effect, and
 * then setting the damage to zero.  Note that the "damage" parameter is
 * lessened by two dice for each square of distance from the center.
 *
 * Note that "polymorph" is dangerous, since a failure in "place_monster()"'
 * may result in a dereference of an invalid pointer.  XXX XXX XXX
 *
 * In this function, "result" messages are postponed until the end, where
 * the "note" string is appended to the monster name, if not NULL.  So,
 * to make a spell have "no effect" just set "note" to NULL.  You should
 * also set "notice" to false, or the player will learn what the spell does.
 *
 * We attempt to return "true" if the player saw anything "useful" happen.
 */
static bool project_m(
    int who, int y, int x, int dd, int ds, int dif, int typ, u32b flg)
{
    int tmp;
    bool suppress_message = !!(flg & PROJECT_SILENT);

    monster_type* m_ptr;
    monster_race* r_ptr;
    monster_lore* l_ptr;

    monster_type* who_ptr = (who == -1) ? PLAYER : &mon_list[who]; // Sil-y
    bool who_vis = (who == -1) ? true : who_ptr->ml;

    int dam = damroll(dd, ds);

    // Monster's skill modifier
    int resistance;

    // Result of opposed check
    int result;

    /* Is the monster "seen"? */
    bool seen = false;

    /* Were the effects "obvious" (if seen)? */
    bool obvious = false;

    /* Were the effects "irrelevant"? */
    bool skipped = false;

    /* Does it alert the monster */
    bool alerting = true;

    /* Polymorph setting (true or false) */
    int do_poly = 0;

    /* Teleport setting (max distance) */
    int do_dist = 0;

    /* Confusion setting (amount to confuse) */
    int do_conf = 0;

    /* Stunning setting (amount to stun) */
    int do_stun = 0;

    /* Slow setting (amount to haste) */
    int do_slow = 0;

    /* Haste setting (amount to haste) */
    int do_haste = 0;

    /* Sleep amount (amount to sleep) */
    int do_sleep = 0;

    /* Fear amount (amount to fear) */
    int do_fear = 0;

    /* Hold the monster name */
    char m_name[80];

    /* Assume no note */
    cptr note = NULL;

    /* Assume a default death */
    cptr note_dies = " dies.";

    /* Unused parameter*/
    (void)flg;

    /* Walls protect monsters */
    if (!cave_floor_bold(y, x))
        return (false);

    /* No monster here */
    if (!(cave_m_idx[y][x] > 0))
        return (false);

    /* Never affect projector */
    if (cave_m_idx[y][x] == who)
        return (false);

    /* Obtain monster info */
    m_ptr = &mon_list[cave_m_idx[y][x]];
    r_ptr = &r_info[m_ptr->r_idx];
    l_ptr = &l_list[m_ptr->r_idx];
    if (m_ptr->ml)
        seen = true;

    /* Get the monster name*/
    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

    /* Some monsters get "destroyed" */
    if (monster_nonliving(r_ptr))
    {
        /* Special note at death */
        note_dies = " is destroyed.";
    }

    /* Monster goes active */
    m_ptr->mflag |= (MFLAG_ACTV);

    /*Mark the monster as attacked by the player*/
    if (who < 0)
        m_ptr->mflag |= (MFLAG_HIT_BY_RANGED);

    /* Analyze the damage type */
    switch (typ)
    {
    /* Acid */
    case GF_ACID:
    {
        if (seen)
            obvious = true;
        break;
    }

    /* Electricity */
    case GF_ELEC:
    {
        if (seen)
            obvious = true;
        if (r_ptr->flags3 & (RF3_RES_ELEC))
        {
            note = " resists.";
            dam = 0;
            if (seen)
                l_ptr->flags3 |= (RF3_RES_ELEC);
        }
        break;
    }

    /* Fire damage */
    case GF_FIRE:
    {
        if (seen)
            obvious = true;
        if (r_ptr->flags3 & (RF3_RES_FIRE))
        {
            note = " resists.";
            dam = 0;
            if (seen)
                l_ptr->flags3 |= (RF3_RES_FIRE);
        }
        if (r_ptr->flags3 & (RF3_HURT_FIRE))
        {
            note = " is badly hurt.";
            dam *= 2;
            if (seen)
                l_ptr->flags3 |= (RF3_HURT_FIRE);
        }
        break;
    }

    /* Cold */
    case GF_COLD:
    {
        if (seen)
            obvious = true;
        if (r_ptr->flags3 & (RF3_RES_COLD))
        {
            note = " resists.";
            dam = 0;
            if (seen)
                l_ptr->flags3 |= (RF3_RES_COLD);
        }
        if (r_ptr->flags3 & (RF3_HURT_COLD))
        {
            note = " is badly hurt.";
            dam *= 2;
            if (seen)
                l_ptr->flags3 |= (RF3_HURT_COLD);
        }
        break;
    }

    /* Poison */
    case GF_POIS:
    {
        if (seen)
            obvious = true;
        if (r_ptr->flags3 & (RF3_RES_POIS))
        {
            note = " resists.";
            dam = 0;
            if (seen)
                l_ptr->flags3 |= (RF3_RES_POIS);
        }
        break;
    }

    /* Sound (use "dam" as amount of stunning) */
    case GF_SOUND:
    {
        obvious = true;

        do_stun = dam;

        /* No "real" damage */
        dam = 0;
        break;
    }

    /* Heal Monster (use "dam" as amount of healing) */
    case GF_HEAL:
    {
        bool healed = true;

        /*does monster need healing?*/
        if (m_ptr->hp == m_ptr->maxhp)
            healed = false;

        if (seen)
            obvious = true;

        /* Monster goes active */
        m_ptr->mflag |= (MFLAG_ACTV);

        /* Heal */
        m_ptr->hp += dam;

        /* No overflow */
        if (m_ptr->hp > m_ptr->maxhp)
            m_ptr->hp = m_ptr->maxhp;

        /* Redraw (later) if needed */
        if (p_ptr->health_who == cave_m_idx[y][x])
            p_ptr->redraw |= (PR_HEALTHBAR);

        /*monster was at full hp to begin*/
        if (!healed)
        {
            obvious = false;
        }

        /* Message */
        else
            note = " looks healthier.";

        // doesn't alert sleeping monsters
        if (m_ptr->alertness < ALERTNESS_UNWARY)
            alerting = false;

        /* No "real" damage */
        dam = 0;
        break;
    }

    /* Speed Monster */
    case GF_SPEED:
    {
        if (seen)
            obvious = true;

        /* Speed up */
        do_haste = dam;

        // doesn't alert sleeping monsters
        if (m_ptr->alertness < ALERTNESS_UNWARY)
            alerting = false;

        /* No "real" damage */
        dam = 0;
        break;
    }

    /* Slow Monster (Use "dif" as difficulty and for duration) */
    case GF_SLOW:
    {
        if (seen)
            obvious = true;

        resistance = monster_skill(m_ptr, S_WIL);
        if (r_ptr->flags3 & (RF3_NO_SLOW))
            resistance += 100;

        // adjust difficulty by the distance to the monster
        result = skill_check(who_ptr,
            dif - distance(p_ptr->py, p_ptr->px, y, x), resistance, m_ptr);

        /* If successful, slow the monster */
        if (result > 0)
        {
            do_slow = result + 10;
        }
        else
        {
            note = " is unaffected!";
            obvious = false;
            if ((seen) && (r_ptr->flags3 & (RF3_NO_SLOW)))
                l_ptr->flags3 |= (RF3_NO_SLOW);
        }

        // doesn't alert sleeping or unaffected monsters
        if ((m_ptr->alertness < ALERTNESS_UNWARY) || (do_slow == 0))
            alerting = false;

        /* No "real" damage */
        dam = 0;

        break;
    }

    /* Sleep (Use "dif" as difficulty and for strength) */
    case GF_SLEEP:
    {
        if (seen)
            obvious = true;

        resistance = monster_skill(m_ptr, S_WIL);
        if (r_ptr->flags3 & (RF3_NO_SLEEP))
            resistance += 100;

        // adjust difficulty by the distance to the monster
        result = skill_check(who_ptr,
            dif + 10 - distance(p_ptr->py, p_ptr->px, y, x), resistance, m_ptr);

        /* If successful, (partially) put the monster to sleep */
        if (result > 0)
        {
            do_sleep = result + 5;
        }
        else
        {
            note = " is unaffected!";
            obvious = false;
            if ((seen) && (r_ptr->flags3 & (RF3_NO_SLEEP)))
                l_ptr->flags3 |= (RF3_NO_SLEEP);
        }

        // doesn't alert monsters
        alerting = false;

        /* No "real" damage */
        dam = 0;

        break;
    }

    /* Confusion (Use "dif" as difficulty and for duration) */
    case GF_CONFUSION:
    {
        if (seen)
            obvious = true;

        resistance = monster_skill(m_ptr, S_WIL);
        if (r_ptr->flags3 & (RF3_NO_CONF))
            resistance += 100;

        // adjust difficulty by the distance to the monster
        result = skill_check(who_ptr,
            dif - distance(p_ptr->py, p_ptr->px, y, x), resistance, m_ptr);

        /* If successful, slow the monster */
        if (result > 0)
        {
            do_conf = result + 10;
        }
        else
        {
            note = " is unaffected!";
            obvious = false;
            if ((seen) && (r_ptr->flags3 & (RF3_NO_CONF)))
                l_ptr->flags3 |= (RF3_NO_CONF);
        }

        // doesn't alert monsters (they are either unaffected or too confused)
        alerting = false;

        /* No "real" damage */
        dam = 0;

        break;
    }

    /* Lite, but only hurts susceptible creatures */
    case GF_LIGHT:
    {
        /* Default: no damage (GF_LIGHT only hurts specific monsters) */
        dam = 0;

        // Must make sure it is viewable (passwall was only used to guarantee
        // wall lighting)
        if (cave_info[y][x] & (CAVE_VIEW))
        {
            int light_level = cave_light[y][x];
            
            /* Hurt by light - ONLY affects HURT_LITE monsters */
            if (r_ptr->flags3 & (RF3_HURT_LITE))
            {
                /* Memorize the effects */
                if (seen)
                    l_ptr->flags3 |= (RF3_HURT_LITE);

                /* Stun and damage work when light level > 2 and player-caused */
                if ((who < 0) && (light_level > 2))
                {
                    int resistance;
                    int result;
                    int actual_dam;
                    int stun_amount;
                    int skill_to_use;
                    
                    /* Determine skill to use for resistance check */
                    /* If dif >= 0, this is Song of Trees (dif contains song score), otherwise use Will */
                    if (dif >= 0)
                        skill_to_use = dif;
                    else
                        skill_to_use = p_ptr->skill_use[S_WIL];
                    
                    /* Get monster's Will resistance */
                    resistance = monster_skill(m_ptr, S_WIL);
                    
                    /* Adjust difficulty by the distance to the player */
                    result = skill_check(PLAYER, skill_to_use, 
                        resistance + 5 + distance(p_ptr->py, p_ptr->px, y, x),
                        m_ptr);
                    
                    /* Stun is applied when monster FAILS Will save (result > 0 means player wins) */
                    /* Stun amount scales with light level */
                    if (result > 0)
                    {
                        stun_amount = damroll(dd, light_level);
                        
                        /* Apply stun */
                        if (stun_amount > 0)
                        {
                            stun_monster(m_ptr, stun_amount);
                            
                            /*possibly update the monster health bar*/
                            if (p_ptr->health_who == cave_m_idx[m_ptr->fy][m_ptr->fx])
                                p_ptr->redraw |= (PR_HEALTHBAR);
                        }
                    }
                    else
                    {
                        /* Monster resisted - no stun */
                        stun_amount = 0;
                    }
                    
                    /* Damage only happens on STRONG Will failure (result >= 10) */
                    /* This represents intense light overwhelming the monster */
                    if (result >= 10)
                    {
                        /* Use light level as dice sides, dd from the attack */
                        actual_dam = damroll(dd, light_level);
                        
                        /* Reduce damage based on how much the monster failed */
                        int raw_dam = actual_dam;
                        actual_dam = (actual_dam * result) / (result + 5);
                        
                        /* Debug logging */
                        if (seen)
                        {
                            log_debug("GF_LIGHT: dd=%d light=%d raw=%d result=%d final=%d stun=%d", 
                                dd, light_level, raw_dam, result, actual_dam, stun_amount);
                        }
                        
                        if (actual_dam > 0)
                        {
                            /* Override dam with actual calculated damage */
                            dam = actual_dam;
                            
                            /* Obvious effect */
                            if (seen)
                                obvious = true;
                            
                            /* Message for visible monsters */
                            if (seen)
                                note = " is seared by radiant light!";
                        }
                        else
                        {
                            dam = 0;
                            
                            /* Stunned but no damage */
                            if (seen)
                                note = " cringes from the light!";
                        }
                    }
                    else if (result > 0)
                    {
                        /* Stunned but not enough to damage */
                        dam = 0;
                        
                        if (seen)
                            note = " cringes from the light!";
                    }
                    else
                    {
                        /* Monster resisted - no stun, no damage */
                        dam = 0;
                        
                        if (seen)
                            note = " resists the light!";
                    }
                }
                else
                {
                    /* Light level too low or not player-caused - no damage or stun */
                    dam = 0;
                }
            }
            else
            {
                /* Not hurt by light - no damage */
                dam = 0;
            }
        }

        // Doesn't alert monsters (there is a seperate function to do this for
        // light)
        alerting = false;

        break;
    }

    /* Dark */
    case GF_DARK:
    {
        if (seen)
            obvious = true;
        if ((r_ptr->flags4 & (RF4_BRTH_DARK)) || (r_ptr->flags3 & (RF3_UNDEAD))
            || (r_ptr->light < 0))
        {
            note = " resists.";
            dam = 0;
        }
        break;
    }

    /* Blasting */
    case GF_KILL_WALL:
    {
        /* Hurt by rock remover */
        if (r_ptr->flags3 & (RF3_STONE))
        {
            /* Notice effect */
            if (seen)
                obvious = true;

            /* Memorize the effects */
            if (seen)
                l_ptr->flags3 |= (RF3_STONE);

            // skill check of Will vs Con * 2
            if (skill_check(PLAYER, dif, monster_stat(m_ptr, A_CON) * 2, m_ptr)
                > 0)
            {
                /* Cute little message */
                note = " partly shatters!";
                note_dies = " shatters!";
            }

            // Will check fails
            else
            {
                note = " resists!";

                /* No damage */
                dam = 0;
            }
        }

        /* Usually, ignore the effects */
        else
        {
            // doesn't alert unaffected monsters
            alerting = false;

            /* No damage */
            dam = 0;
        }

        break;
    }

    /* Teleport monster (Use "dam" as "power") */
    case GF_AWAY_ALL:
    {
        /* Obvious */
        if (seen)
            obvious = true;

        /* Prepare to teleport */
        do_dist = dam;

        /* No "real" damage */
        dam = 0;
        break;
    }

    /* Fear (Use "dif" as difficulty and for duration) */
    case GF_FEAR:
    {
        resistance = monster_skill(m_ptr, S_WIL);
        if (r_ptr->flags3 & (RF3_NO_FEAR))
            resistance += 100;

        // adjust difficulty by the distance to the monster
        result = skill_check(who_ptr,
            dif + 10 - distance(p_ptr->py, p_ptr->px, y, x), resistance, m_ptr);

        if (result > 0)
        {
            /* Obvious */
            if (seen)
                obvious = true;

            /* Apply some fear */
            do_fear = result * 20;
        }
        else
        {
            // Doesn't alert unaffected monsters
            alerting = false;

            /* No obvious effect */
            note = " is unaffected!";
            obvious = false;

            if ((seen) && (r_ptr->flags3 & (RF3_NO_FEAR)))
                l_ptr->flags3 |= (RF3_NO_FEAR);
        }

        /* No "real" damage */
        dam = 0;
        break;
    }

    /* No effect */
    case GF_NOTHING:
    {
        break;
    }

    /* Default */
    default:
    {
        /* Irrelevant */
        skipped = true;

        /* No damage */
        dam = 0;

        break;
    }
    }

    /* Absolutely no effect */
    if (skipped)
        return (false);

    /* "Unique" monsters cannot be polymorphed */
    if (r_ptr->flags1 & (RF1_UNIQUE))
        do_poly = false;

    /* "Unique" monsters can only be "killed" by the player */
    // if (r_ptr->flags1 & (RF1_UNIQUE))
    //{
    //	/* Uniques may only be killed by the player */
    //	if ((who > 0) && (dam > m_ptr->hp)) dam = m_ptr->hp;
    //}

    /* Check for death */
    if (dam > m_ptr->hp)
    {
        /* Extract method of death */
        note = note_dies;
    }

    /* Mega-Hack -- Handle "polymorph" -- monsters get a saving throw */
    else if (do_poly && (dieroll(90) > r_ptr->level))
    {
        /* Default -- assume no polymorph */
        note = " is unaffected!";

        /* Pick a "new" monster race */
        tmp = poly_r_idx(m_ptr);

        /* Handle polymorph */
        if (tmp != m_ptr->r_idx)
        {
            /* Obvious */
            if (seen)
                obvious = true;

            /* Monster polymorphs */
            note = " changes!";

            /* Turn off the damage */
            dam = 0;

            /* "Kill" the "old" monster */
            delete_monster_idx(cave_m_idx[y][x]);

            /* Create a new monster (no groups) */
            (void)place_monster_aux(y, x, tmp, false, false);

            /* Hack -- Assume success XXX XXX XXX */

            /* Hack -- Get new monster */
            m_ptr = &mon_list[cave_m_idx[y][x]];

            /* Hack -- Get new race */
            r_ptr = &r_info[m_ptr->r_idx];
        }
    }

    /* Handle "teleport" */
    else if (do_dist)
    {
        /* no teleporting on certain levels */
        if ((p_ptr->depth != 0) && (p_ptr->depth != MORGOTH_DEPTH))
        {
            /* Obvious */
            if (seen)
                obvious = true;

            /* Message */
            note = " disappears!";

            /* Teleport */
            teleport_away(cave_m_idx[y][x], do_dist);

            /* Hack -- get new location */
            y = m_ptr->fy;
            x = m_ptr->fx;
        }
    }

    /* Stunning */
    else if (do_stun)
    {
        /* Obvious */
        if (seen)
            obvious = true;

        /* Get confused */
        if (m_ptr->stunned)
            note = " is more dazed.";
        else
            note = " is dazed.";

        /*some creatures are resistant to stunning*/
        if (r_ptr->flags3 & RF3_NO_STUN)
        {
            /*mark the lore*/
            if (seen)
                l_ptr->flags3 |= (RF3_NO_STUN);

            note = " is unaffected!";
        }

        /* Apply stun */
        else
            stun_monster(m_ptr, do_stun);

        /*possibly update the monster health bar*/
        if (p_ptr->health_who == cave_m_idx[m_ptr->fy][m_ptr->fx])
            p_ptr->redraw |= (PR_HEALTHBAR);
    }

    /* Confusion  */
    else if (do_conf)
    {
        /* Obvious */
        if (seen)
            obvious = true;

        /* Generate message */
        if (m_ptr->confused)
            note = " looks more confused.";
        else
            note = " looks confused.";

        tmp = m_ptr->confused + do_conf;

        /* Apply confusion */
        m_ptr->confused += (tmp < 200) ? tmp : 200;

        if (p_ptr->health_who == cave_m_idx[m_ptr->fy][m_ptr->fx])
            p_ptr->redraw |= (PR_HEALTHBAR);
    }

    /*Slowing*/
    else if (do_slow)
    {
        /* Increase slowing */
        tmp = m_ptr->slowed + do_slow;

        /* set or add to slow counter */
        set_monster_slow(cave_m_idx[m_ptr->fy][m_ptr->fx], tmp, seen);
    }

    /* Hasting */
    else if (do_haste)
    {
        /* Increase haste */
        tmp = m_ptr->hasted + do_haste;

        /* set or add to slow counter */
        set_monster_haste(cave_m_idx[m_ptr->fy][m_ptr->fx], tmp, seen);
    }

    /* Fear */
    if (do_fear)
    {
        /* Decrease temporary morale */
        m_ptr->tmp_morale -= do_fear;
    }

    // update combat info
    if ((dam > 0) && m_ptr->ml)
    {
        int combat_dd = dd;
        int combat_ds = ds;

        if (typ == GF_LIGHT)
            combat_ds = cave_light[y][x];

        update_combat_rolls1b(who_ptr, m_ptr, who_vis);
        update_combat_rolls2(combat_dd, combat_ds, dam, -1, -1, 0, 0, typ, false);
    }

    /* If another monster did the damage, hurt the monster by hand */
    if (who > 0)
    {
        /* Redraw (later) if needed */
        if (p_ptr->health_who == cave_m_idx[y][x])
            p_ptr->redraw |= (PR_HEALTHBAR);

        /* Monster goes active */
        m_ptr->mflag |= (MFLAG_ACTV);

        /* Hurt the monster */
        m_ptr->hp -= dam;

        if (dam > 0)
            maybe_update_morgoth_state_from_hp(m_ptr);

        /* Dead monster */
        if (m_ptr->hp <= 0)
        {
            /* Song of Trees: trolls slain by radiant light crumble into rubble (Kemenrauko-style). */
            if ((typ == GF_LIGHT) && (who < 0) && (dif >= 0)
                && (r_ptr->flags3 & RF3_TROLL) && !cave_stair_bold(y, x))
            {
                cave_set_feat(y, x, FEAT_RUBBLE);
            }

            /* Generate treasure, etc */
            monster_death(cave_m_idx[y][x]);

            /* Delete the monster */
            delete_monster_idx(cave_m_idx[y][x]);

            /* Give detailed messages if destroyed */
            if ((note) && (seen))
            {
                /* dump the note*/
                if (!suppress_message)
                    msg_format("%^s%s", m_name, note);
            }

            else
                death_count++;
        }

        /* Damaged monster */
        else
        {
            // Alert it
            make_alert(m_ptr);

            /* Give detailed messages if visible or destroyed */
            if (note && seen)
            {
                /* dump the note*/
                if (!suppress_message)
                    msg_format("%^s%s", m_name, note);
            }

            /* Hack -- Pain message */
            else if (dam > 0)
                message_pain(cave_m_idx[y][x], dam);

            /* Hack -- handle sleep */
            if (do_sleep)
            {
                set_alertness(m_ptr, m_ptr->alertness - do_sleep);
            }
        }
    }

    /* If the player did it, give him experience, check fear */
    else
    {
        /*hack - only give message if seen*/
        if (!seen)
            note_dies = "";

        /* Any player-sourced effect that touches Morgoth breaks Nienna's charge */
        if (who < 0 && m_ptr->r_idx == R_IDX_MORGOTH) {
            niena_mark_morgoth_attack();
        }

        /* Check for oath breaking before applying damage */
        if (who < 0 && dam > 0) // Player-caused damage
        {
            /* All player-caused attacks break Valor on hit */
            if (m_ptr->ml && cowardly_attack(m_ptr))
            {
                do_cmd_note("Broke your oath", p_ptr->depth);
                apply_oath_breaking_curse(OATH_VALOROUS);
                p_ptr->oaths_broken |= OATH_VALOROUS_FLAG;
            }

            break_mercy_oath(m_ptr, dam);
        }

        /* Hurt the monster, check for death */
        if (mon_take_hit(cave_m_idx[y][x], dam, note_dies, who))
        {
            /* Note death */
            if (!seen)
                death_count++;
        }

        /* Damaged monster */
        else
        {
            // Alert it, if there has been no damage to alert it so far
            if (alerting && (dam == 0))
                make_alert(m_ptr);

            /* Give detailed messages if visible or destroyed */
            if (note && seen)
            {
                if (!suppress_message)
                    msg_format("%^s%s", m_name, note);
            }

            /* Hack -- Pain message */
            else if (dam > 0)
                message_pain(cave_m_idx[y][x], dam);

            /* Take note */
            if ((do_fear) && (m_ptr->ml) && (!suppress_message))
            {
                /* Message */
                message_format(MSG_FLEE, m_ptr->r_idx, "%^s cowers.", m_name);
            }

            /* Hack -- handle sleep */
            if (do_sleep)
            {
                set_alertness(m_ptr, m_ptr->alertness - do_sleep);
            }
        }
    }

    /* Verify this code XXX XXX XXX */

    /* Update the monster */
    update_mon(cave_m_idx[y][x], false);

    /* Redraw the monster grid */
    dungeon_mark_map_for_redraw();

    /* Update monster recall window */
    if (p_ptr->monster_race_idx == m_ptr->r_idx)
    {
        /* Window stuff */
        p_ptr->window |= (PW_MONSTER);
    }

    /* Track it */
    project_m_n++;
    project_m_x = x;
    project_m_y = y;

    /*
     * If this is the first monster hit, the spell was capable
     * of causing damage, and the player was the source of the spell,
     * make noise. -LM-
     */
    if ((project_m_n == 1) && (who <= 0) && (dam))
    {
        stealth_score -= 0;
    }

    /* Return "Anything seen?" */
    return (obvious);
}

/*
 * Helper function for "project()" below.
 *
 * Handle a beam/bolt/ball causing damage to the player.
 *
 * This routine takes a "source monster" (by index), a "distance", a default
 * "damage", and a "damage type".  See "project_m()" above.
 *
 * If "rad" is non-zero, then the blast was centered elsewhere, and the damage
 * is reduced (see "project_m()" above).  This can happen if a monster breathes
 * at the player and hits a wall instead.
 *
 * We return "true" if any "obvious" effects were observed.
 *
 * Actually, for historical reasons, we just assume that the effects were
 * obvious.  XXX XXX XXX
 */
static bool project_p(int who, int y, int x, int dd, int ds, int dif, int typ)
{
    /* Hack -- assume obvious */
    bool obvious = true;

    /* Player blind-ness */
    bool blind = (p_ptr->blind ? true : false);

    /* Source monster */
    monster_type* m_ptr;
    monster_race* r_ptr;

    /* Monster name (for attacks) */
    char m_name[80];

    /* Monster name (for damage) */
    char killer[80];

    int dam;

    bool do_disturb = true;

    // Sil-y: unusued parameter, casting it to soothe compilation warnings
    (void)dif;

    /* No player here */
    if (!(cave_m_idx[y][x] < 0))
        return (false);

    /* Never affect projector */
    if (cave_m_idx[y][x] == who)
        return (false);

    /* Get the source monster */
    m_ptr = &mon_list[who];

    /* Get the monster race. */
    r_ptr = &r_info[m_ptr->r_idx];

    if (who > 0 && who < mon_max) {
        killer_mark_monster(m_ptr);
    } else {
        killer_mark_other(SCORE_KILLER_OTHER);
    }

    /* Get the monster name */
    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

    /* Get the monster's real name */
    monster_desc(killer, sizeof(killer), m_ptr, 0x88);

    dam = damroll(dd, ds);

    // generate the display messages for undodgable attacks
    if ((dam > 0) && (typ != GF_ARROW) && (typ != GF_BOULDER)
        && (typ != GF_WEB))
    {
        update_combat_rolls1b(m_ptr, PLAYER, m_ptr->ml);

        if ((typ != GF_FIRE) && (typ != GF_COLD) && (typ != GF_POIS)
            && (typ != GF_DARK))
        {
            update_combat_rolls2(dd, ds, dam, -1, -1, 0, 0, typ, false);
        }
    }

    /* Analyze the damage */
    switch (typ)
    {
    /* Standard damage -- hurts inventory too */
    case GF_ACID:
    {
        if (blind)
            msg_print("You are hit by acid!");
        acid_dam(dam, dd, dd * ds, dam, killer);
        break;
    }

    /* Standard damage -- hurts inventory too */
    case GF_ELEC:
    {
        if (blind)
            msg_print("You are hit by lightning!");
        elec_dam(dam, dd, dd * ds, dam, killer);
        break;
    }

    /* Standard damage -- hurts inventory too */
    case GF_FIRE:
    {
        if (blind)
            msg_print("You are hit by fire!");
        fire_dam_pure(dd, ds, true, killer);
        break;
    }

    /* Standard damage -- hurts inventory too */
    case GF_COLD:
    {
        if (blind)
            msg_print("You are hit by cold!");
        cold_dam_pure(dd, ds, true, killer);
        break;
    }

    /* Dark  */
    case GF_DARK:
    {
        if (blind)
            msg_print("You are hit by something!");
        dark_dam_pure(dd, ds, true, killer);
        break;
    }

    /* Weak Dark -- nothing! */
    case GF_DARK_WEAK:
    {
        do_disturb = false;
        obvious = false;
        break;
    }

    /* Posion */
    case GF_POIS:
    {
        if (blind)
            msg_print("You are hit by poison!");
        (void)pois_dam_pure(dd, ds, true);
        break;
    }

    /* Arrow */
    case GF_ARROW:
    {
        int total_attack_mod, total_evasion_mod, crit_bonus_dice, hit_result;
        int total_dd, total_ds;
        int prt, net_dam, weight;

        // attacks with GF_ARROW will require an attack roll

        // determine the monster's attack score
        total_attack_mod = total_monster_attack(m_ptr, r_ptr->spell_power);

        // determine the player's evasion score
        total_evasion_mod = total_player_evasion(m_ptr, false);

        // target only gets half the evasion modifier against archery
        total_evasion_mod /= 2;

        // simulate weights of longbows and shortbows
        if (ds >= 11)
            weight = 30;
        else
            weight = 20;

        // perform the hit roll
        hit_result = hit_roll(
            total_attack_mod, total_evasion_mod, m_ptr, PLAYER, true);

        if (hit_result > 0)
        {
            crit_bonus_dice = crit_bonus(
                hit_result, weight, &r_info[0], S_ARC, false, m_ptr, NULL);
            total_dd = dd + crit_bonus_dice;
            total_ds = ds;

            dam = damroll(total_dd, total_ds);

            // armour is effective against GF_ARROW
            prt = protection_roll(GF_HURT, false);
            net_dam = (dam - prt > 0) ? (dam - prt) : 0;

            if (blind)
            {
                msg_print("You are hit by something sharp.");
            }
            else
            {
                if (net_dam > 0)
                {
                    if (crit_bonus_dice == 0)
                    {
                        msg_print("It hits you.");
                    }
                    else
                    {
                        msg_print("It hits!");
                    }
                }
            }

            update_combat_rolls2(
                total_dd, total_ds, dam, -1, -1, prt, 100, GF_HURT, false);
            display_hit(p_ptr->py, p_ptr->px, net_dam, GF_HURT, p_ptr->is_dead);

            if (net_dam)
            {
                take_hit(net_dam, killer);

                // deal with crippling shot ability
                if ((r_ptr->flags2 & (RF2_CRIPPLING)) && (crit_bonus_dice >= 1)
                    && (net_dam > 0))
                {
                    // Sil-y: ideally we'd use a call to allow_player_slow()
                    // here, but that doesn't
                    //        work as it can't take the level of the critical
                    //        into account. Sadly my solution doesn't let you ID
                    //        free action items.
                    int difficulty
                        = p_ptr->skill_use[S_WIL] + (p_ptr->free_act * 10);

                    if (skill_check(
                            m_ptr, crit_bonus_dice * 4, difficulty, PLAYER)
                        > 0)
                    {
                        monster_lore* l_ptr = &l_list[m_ptr->r_idx];

                        // remember that the monster can do this
                        if (m_ptr->ml)
                            l_ptr->flags2 |= (RF2_CRIPPLING);

                        msg_format("The shot tears into your thigh!");

                        // slow the player
                        set_slow(p_ptr->slow + crit_bonus_dice);
                    }
                }
            }

            /* Make some noise */
            monster_perception(true, false, -5);
        }

        break;
    }

    /* Boulder */
    /* mostly the same as GF_ARROW, but doing 6d4 damage instead*/
    case GF_BOULDER:
    {
        int total_attack_mod, total_evasion_mod, crit_bonus_dice, hit_result;
        int total_dd, total_ds;
        int prt, net_dam;

        // attacks with GF_BOULDER will require an attack roll

        // determine the monster's attack score
        total_attack_mod = total_monster_attack(m_ptr, r_ptr->spell_power);

        // determine the player's evasion score
        total_evasion_mod = total_player_evasion(m_ptr, false);

        // perform the hit roll
        hit_result = hit_roll(
            total_attack_mod, total_evasion_mod, m_ptr, PLAYER, true);

        if (hit_result > 0)
        {
            crit_bonus_dice
                = crit_bonus(hit_result, 100, &r_info[0], S_ARC, true, m_ptr, NULL);
            total_dd = dd + crit_bonus_dice;
            total_ds = ds;

            dam = damroll(total_dd, total_ds);

            // armour is effective against GF_BOULDER
            prt = protection_roll(GF_HURT, false);
            net_dam = (dam - prt > 0) ? (dam - prt) : 0;

            if (blind)
            {
                msg_print("You are hit by something very heavy.");
            }
            else
            {
                if (net_dam > 0)
                {
                    if (crit_bonus_dice == 0)
                    {
                        msg_print("It hits you.");
                    }
                    else
                    {
                        msg_print("It hits!");
                    }
                }
            }

            update_combat_rolls2(
                total_dd, total_ds, dam, -1, -1, prt, 100, GF_HURT, false);
            display_hit(p_ptr->py, p_ptr->px, net_dam, GF_HURT, p_ptr->is_dead);

            if (net_dam)
            {
                take_hit(net_dam, killer);
            }

            /* Make some noise */
            monster_perception(true, false, -10);
        }

        break;
    }

    case GF_WEB:
    {
        int total_attack_mod, total_evasion_mod, hit_result;
        // attacks with GF_WEB will require an attack roll

        // determine the monster's attack score
        total_attack_mod = total_monster_attack(m_ptr, r_ptr->spell_power);

        // determine the player's evasion score
        total_evasion_mod = total_player_evasion(m_ptr, false);

        // perform the hit roll
        hit_result = hit_roll(
            total_attack_mod, total_evasion_mod, m_ptr, PLAYER, true);

        if (hit_result > 0)
        {
            int feat = cave_feat[p_ptr->py][p_ptr->px];
            bool can_web = (feat == FEAT_FLOOR || feat == FEAT_TRAP_WEB);

            if (can_web)
            {
                if (blind)
                {
                    msg_print("Something sticky falls over you.");
                }
                else
                {
                    msg_print("You are enveloped in a thick web.");
                }

                cave_set_feat(p_ptr->py, p_ptr->px, FEAT_TRAP_WEB);
            }
            else
            {
                if (blind)
                {
                    msg_print("Something sticky splatters nearby.");
                }
                else
                {
                    msg_print("The web cannot take hold here.");
                }
            }
        }

        break;
    }

    /* Sound (use "dam" as stunning) */
    case GF_SOUND:
    {
        if (blind)
            msg_print("You are hit by a cacophony of sound!");
        if (allow_player_stun(m_ptr))
        {
            (void)set_stun(p_ptr->stun + dam);
        }
        else
        {
            msg_print("You are unfazed.");
        }
        sound_dam(dam, dd, dd * ds, dam);
        break;
    }

    /* Does nothing */
    case GF_NOTHING:
    {
        do_disturb = false;
        obvious = false;
        break;
    }

    /* Default */
    default:
    {
        /* No damage */
        dam = 0;

        break;
    }
    }

    /* Disturb */
    if (do_disturb)
        disturb(1, 0);

    /* Return "Anything seen?" */
    return (obvious);
}

/*
 * Calculate and store the arcs used to make starbursts.
 */
static void calc_starburst(
    int height, int width, byte* arc_first, byte* arc_dist, int* arc_num)
{
    int i;
    int size, dist, vert_factor;
    int degree_first, center_of_arc;

    /* Note the "size" */
    size = 2 + div_round(width + height, 22);

    /* Ask for a reasonable number of arcs. */
    *arc_num = 8 + (height * width / 80);
    *arc_num = rand_spread(*arc_num, 3);
    if (*arc_num < 8)
        *arc_num = 8;
    if (*arc_num > 45)
        *arc_num = 45;

    /* Determine the start degrees and expansion distance for each arc. */
    for (degree_first = 0, i = 0; i < *arc_num; i++)
    {
        /* Get the first degree for this arc (using 180-degree circles). */
        arc_first[i] = degree_first;

        /* Get a slightly randomized start degree for the next arc. */
        degree_first += div_round(180, *arc_num);

        /* Do not entirely leave the usual range */
        if (degree_first < 180 * (i + 1) / *arc_num)
            degree_first = 180 * (i + 1) / *arc_num;
        if (degree_first > (180 + *arc_num) * (i + 1) / *arc_num)
            degree_first = (180 + *arc_num) * (i + 1) / *arc_num;

        /* Get the center of the arc (convert from 180 to 360 circle). */
        center_of_arc = degree_first + arc_first[i];

        /* Get arc distance from the horizontal (0 and 180 degrees) */
        if (center_of_arc <= 90)
            vert_factor = center_of_arc;
        else if (center_of_arc >= 270)
            vert_factor = ABS(center_of_arc - 360);
        else
            vert_factor = ABS(center_of_arc - 180);

        /*
         * Usual case -- Calculate distance to expand outwards.  Pay more
         * attention to width near the horizontal, more attention to height
         * near the vertical.
         */
        dist = ((height * vert_factor) + (width * (90 - vert_factor))) / 90;

        /* Randomize distance (should never be greater than radius) */
        arc_dist[i] = rand_range(dist / 4, dist / 2);

        /* Keep variability under control (except in special cases). */
        if ((dist != 0) && (i != 0))
        {
            int diff = arc_dist[i] - arc_dist[i - 1];

            if (ABS(diff) > size)
            {
                if (diff > 0)
                    arc_dist[i] = arc_dist[i - 1] + size;
                else
                    arc_dist[i] = arc_dist[i - 1] - size;
            }
        }
    }

    /* Neaten up final arc of circle by comparing it to the first. */
    if (true)
    {
        int diff = arc_dist[*arc_num - 1] - arc_dist[0];

        if (ABS(diff) > size)
        {
            if (diff > 0)
                arc_dist[*arc_num - 1] = arc_dist[0] + size;
            else
                arc_dist[*arc_num - 1] = arc_dist[0] - size;
        }
    }
}

/*
 * Generic "beam"/"bolt"/"ball" projection routine.
 *
 * Input:
 *   who: Index of "source" monster (negative for "player")
 *   rad: Radius of explosion (0 = beam/bolt, 1 to 9 = ball)
 *   y,x: Target location (or location to travel "towards")
 *   dam: Base damage roll to apply to affected monsters (or player)
 *   typ: Type of damage to apply to monsters (and objects)
 *   flg: Extra bit flags (see PROJECT_xxxx in "defines.h")
 *   degrees: How wide an arc spell is (in degrees).
 *   uniform: uniform means no damage reduction with range, otherwise it is one
 * die per square.
 *
 * Return:
 *   true if any "effects" of the projection were observed, else false
 *
 * At present, there are five major types of projections:
 *
 * Point-effect projection:  (no PROJECT_BEAM flag, radius of zero, and either
 *   jumps directly to target or has a single source and target grid)
 * A point-effect projection has no line of projection, and only affects one
 *   grid.  It is used for most area-effect spells (like dispel evil) and
 *   pinpoint strikes.
 *
 * Bolt:  (no PROJECT_BEAM flag, radius of zero, has to travel from source to
 *   target)
 * A bolt travels from source to target and affects only the final grid in its
 *   projection path.  If given the PROJECT_STOP flag, it is stopped by any
 *   monster or character in its path (at present, all bolts use this flag).
 *
 * Beam:  (PROJECT_BEAM)
 * A beam travels from source to target, affecting all grids passed through
 *   with full damage.  It is never stopped by monsters in its path.  Beams
 *   may never be combined with any other projection type.
 *
 * Ball:  (positive radius, unless the PROJECT_ARC flag is set)
 * A ball travels from source towards the target, and always explodes.  Unless
 *   specified, it does not affect wall grids, but otherwise affects any grids
 *   in LOS from the center of the explosion.
 * If used with a direction, a ball will explode on the first occupied grid in
 *   its path.  If given a target, it will explode on that target.  If a
 *   wall is in the way, it will explode against the wall.  If a ball reaches
 *   MAX_RANGE without hitting anything or reaching its target, it will
 *   explode at that point.
 *
 * Arc:  (positive radius, with the PROJECT_ARC flag set)
 * An arc is a portion of a source-centered ball that explodes outwards
 *   towards the target grid.  Like a ball, it affects all non-wall grids in
 *   LOS of the source in the explosion area.  The width of arc spells is con-
 *   trolled by degrees.
 * An arc is created by rejecting all grids that form the endpoints of lines
 *   whose angular difference (in degrees) from the centerline of the arc is
 *   greater than one-half the input "degrees".  See the table "get_
 *   angle_to_grid" in "util.c" for more information.
 * Note:  An arc with a value for degrees of zero is actually a beam of
 *   defined length.
 *
 * Projections that affect all monsters in LOS are handled through the use
 *   of "project_los()", which applies a single-grid projection to individual
 *   monsters.  Projections that light up rooms or affect all monsters on the
 *   level are more efficiently handled through special functions.
 *
 *
 * Variations:
 *
 * PROJECT_STOP forces a path of projection to stop at the first occupied
 *   grid it hits.  This is used with bolts, and also by ball spells
 *   travelling in a specific direction rather than towards a target.
 *
 * PROJECT_THRU allows a path of projection towards a target to continue
 *   past that target.
 *
 * PROJECT_JUMP allows a projection to immediately set the source of the pro-
 *   jection to the target.  This is used for all area effect spells (like
 *   dispel evil), and can also be used for bombardments.
 *
 * PROJECT_WALL allows a projection, not just to affect one layer of any
 *   passable wall (rubble, trees), but to affect the surface of any wall.
 *   Certain projection types always have this flag.
 *
 * PROJECT_PASS allows projections to ignore walls completely.
 *   Certain projection types always have this flag.
 *
 * PROJECT_HIDE erases all graphical effects, making the projection
 *   invisible.
 *
 * PROJECT_GRID allows projections to affect terrain features.
 *
 * PROJECT_ITEM allows projections to affect objects on the ground.
 *
 * PROJECT_KILL allows projections to affect monsters.
 *
 * PROJECT_PLAY allows projections to affect the player.
 *
 * degrees controls the width of arc spells.  With a value for
 *   degrees of zero, arcs act like beams of defined length.
 *
 * Implementation notes:
 *
 * If the source grid is not the same as the target, we project along the path
 *   between them.  Bolts stop if they hit anything, beams stop if they hit a
 *   wall, and balls and arcs may exhibit either behavior.  When they reach
 *   the final grid in the path, balls and arcs explode.  We do not allow beams
 *   to be combined with explosions.
 * Balls affect all floor grids in LOS (optionally, also wall grids adjacent
 *   to a grid in LOS) within their radius.  Arcs do the same, but only within
 *   their cone of projection.
 * Because affected grids are only scanned once, and it is really helpful to
 *   have explosions that travel outwards from the source, they are sorted by
 *   distance.  For each distance, an adjusted damage is calculated.
 * In successive passes, the code then displays explosion graphics, erases
 *   these graphics, marks terrain for possible later changes, affects
 *   objects, monsters, the character, and finally changes features and
 *   teleports monsters and characters in marked grids.
 *
 *
 * Usage and graphics notes:
 *
 * If the option "fresh_before" is on, or the delay factor is anything other
 * than zero, bolt and explosion pictures will be momentarily shown on screen.
 *
 * Only 256 grids can be affected per projection, limiting the effective
 * radius of standard ball attacks to nine units (diameter nineteen).  Arcs
 * can have larger radii; an arc capable of going out to range 20 should not
 * be wider than 70 degrees.
 *
 * Balls must explode BEFORE hitting walls, or they would affect monsters on
 * both sides of a wall.
 *
 * Note that for consistency, we pretend that the bolt actually takes time
 * to move from point A to point B, even if the player cannot see part of the
 * projection path.  Note that in general, the player will *always* see part
 * of the path, since it either starts at the player or ends on the player.
 *
 * Hack -- we assume that every "projection" is "self-illuminating".
 *
 * Hack -- when only a single monster is affected, we automatically track
 * (and recall) that monster, unless "PROJECT_JUMP" is used.
 *
 * Note that we must call "handle_stuff()" after affecting terrain features
 * in the blast radius, in case the illumination of the grid was changed,
 * and "update_view()" and "update_monsters()" need to be called.
 */
bool project(int who, int rad, int y0, int x0, int y1, int x1, int dd, int ds,
    int dif, int typ, u32b flg, int degrees, bool uniform)
{
    int i, j, k;
    int dist = 0;

    u32b dam_temp;
    int centerline = 0;

    int y = y0;
    int x = x0;
    int n1y = 0;
    int n1x = 0;
    int y2, x2;

    int msec = op_ptr->delay_factor * op_ptr->delay_factor;

    /* Assume the player sees nothing */
    bool notice = false;

    /* Assume the player has seen nothing */
    bool visual = false;

    /* Assume the player has seen no blast grids */
    bool drawn = false;

    /* Is the player blind? */
    bool blind = (p_ptr->blind ? true : false);

    /* Number of grids in the "path" */
    int path_n = 0;

    /* Actual grids in the "path" */
    u16b path_g[512];

    /* Number of grids in the "blast area" (including the "beam" path) */
    int grids = 0;

    /* Coordinates of the affected grids */
    byte gx[256], gy[256];

    /* Distance to each of the affected grids. */
    byte gd[256];

    /* Precalculated damage values for each distance. */
    int dam_at_dist[MAX_RANGE + 1];

    /*
     * Starburst projections only --
     * Holds first degree of arc, maximum effect distance in arc.
     */
    byte arc_first[45];
    byte arc_dist[45];

    /* Number (max 45) of arcs. */
    int arc_num = 0;

    int degree, max_dist;

    /* Hack -- Flush any pending output */
    handle_stuff();

    /* Make certain that the radius is not too large */
    if (rad > MAX_SIGHT)
        rad = MAX_SIGHT;

    /* Some projection types always PROJECT_WALL. */
    if ((typ == GF_KILL_WALL) || (typ == GF_KILL_DOOR))
    {
        flg |= (PROJECT_WALL);
    }

    /* Hack -- Jump to target, but require a valid target */
    if ((flg & (PROJECT_JUMP)) && (y1) && (x1))
    {
        y0 = y1;
        x0 = x1;

        /* Clear the flag */
        flg &= ~(PROJECT_JUMP);
    }

    /* If a single grid is both source and destination, store it. */
    if ((x1 == x0) && (y1 == y0))
    {
        gy[grids] = y0;
        gx[grids] = x0;
        gd[grids++] = 0;
    }

    /* Otherwise, unless an arc or a star, travel along the projection path. */
    else if (!(flg & (PROJECT_ARC | PROJECT_STAR)))
    {
        /* Determine maximum length of projection path */
        if (flg & (PROJECT_BOOM))
            dist = MAX_RANGE;
        else if (rad <= 0)
            dist = MAX_RANGE;
        else
            dist = rad;

        /* Calculate the projection path */
        path_n = project_path(path_g, dist, y0, x0, &y1, &x1, flg);
        if (path_n > 0)
        {
            app_session_note_animation(app_session_current(),
                APP_ANIMATION_HINT_PROJECTILE, typ,
                APP_PACK_COORD(y0, x0), APP_PACK_COORD(y1, x1), flg,
                APP_SNAPSHOT_INVALIDATE_MAP);
        }

        /* Project along the path */
        for (i = 0; i < path_n; ++i)
        {
            int oy = y;
            int ox = x;

            int ny = GRID_Y(path_g[i]);
            int nx = GRID_X(path_g[i]);

            /* Hack -- Balls explode before reaching walls. */
            if ((flg & (PROJECT_BOOM)) && (!cave_floor_bold(ny, nx)))
            {
                break;
            }

            /* Advance */
            y = ny;
            x = nx;

            /* If a beam, collect all grids in the path. */
            if (flg & (PROJECT_BEAM))
            {
                gy[grids] = y;
                gx[grids] = x;
                gd[grids++] = 0;
            }

            /* Otherwise, collect only the final grid in the path. */
            else if (i == path_n - 1)
            {
                gy[grids] = y;
                gx[grids] = x;
                gd[grids++] = 0;
            }

            /* Only do visuals if requested */
            if (!blind && !(flg & (PROJECT_HIDE)))
            {
                /* Only do visuals if the player can "see" the projection */
                if (panel_contains(y, x) && player_has_los_bold(y, x))
                {
                    u16b p;

                    byte a;
                    char c;

                    /* Obtain the bolt pict */
                    p = bolt_pict(oy, ox, y, x, typ);

                    /* Extract attr/char */
                    a = PICT_A(p);
                    c = PICT_C(p);

                    /* Present the semantic projectile animation frame. */
                    (void)a;
                    (void)c;
                    if (op_ptr->delay_factor)
                        platform_frame_present();

                    /* Delay */
                    platform_frame_delay_ms((u32b)msec);

                    if (op_ptr->delay_factor)
                        platform_frame_present();

                    /* Hack -- Activate delay */
                    visual = true;
                }

                /* Hack -- Always delay for consistency */
                else if (visual)
                {
                    /* Delay for consistency */
                    platform_frame_delay_ms((u32b)msec);
                }
            }
        }
    }

    /* Save the "blast epicenter" */
    y2 = y;
    x2 = x;

    /* Beams have already stored all the grids they will affect. */
    if (flg & (PROJECT_BEAM))
    {
        /* No special actions */
    }

    /* Handle explosions */
    else if (flg & (PROJECT_BOOM))
    {
        /* Some projection types always PROJECT_WALL. */
        if (typ == GF_ACID)
        {
            /* Note that acid only affects monsters if it melts the wall. */
            flg |= (PROJECT_WALL);
        }

        /* Pre-calculate some things for starbursts. */
        if (flg & (PROJECT_STAR))
        {
            calc_starburst(
                1 + rad * 2, 1 + rad * 2, arc_first, arc_dist, &arc_num);

            /* Mark the area nearby -- limit range, ignore rooms */
            spread_cave_temp(y0, x0, rad, false);
        }

        /* Pre-calculate some things for arcs. */
        if (flg & (PROJECT_ARC))
        {
            /* The radius of arcs cannot be more than 20 */
            if (rad > 20)
                rad = 20;

            /* Reorient the grid forming the end of the arc's centerline. */
            n1y = y1 - y0 + 20;
            n1x = x1 - x0 + 20;

            /* Correct overly large or small values */
            if (n1y > 40)
                n1y = 40;
            if (n1x > 40)
                n1x = 40;
            if (n1y < 0)
                n1y = 0;
            if (n1x < 0)
                n1x = 0;

            /* Get the angle of the arc's centerline */
            centerline = 90 - get_angle_to_grid[n1y][n1x];
        }

        /*
         * If the center of the explosion hasn't been
         * saved already, save it now.
         */
        if (grids == 0)
        {
            gy[grids] = y2;
            gx[grids] = x2;
            gd[grids++] = 0;
        }

        /*
         * Scan every grid that might possibly
         * be in the blast radius.
         */
        for (y = y2 - rad; y <= y2 + rad; y++)
        {
            for (x = x2 - rad; x <= x2 + rad; x++)
            {
                /* Center grid has already been stored. */
                if ((y == y2) && (x == x2))
                    continue;

                /* Precaution: Stay within area limit. */
                if (grids >= 255)
                    break;

                /* Ignore "illegal" locations */
                if (!in_bounds(y, x))
                    continue;

                /* This is a wall grid (whether passable or not). */
                if (!cave_floor_bold(y, x))
                {
                    /* Spell with PROJECT_PASS ignore walls */
                    if (!(flg & (PROJECT_PASS)))
                    {
                        /* This grid is passable, or PROJECT_WALL is active */
                        if ((flg & (PROJECT_WALL)) || (cave_floor_bold(y, x)))
                        {
                            /* Allow grids next to grids in LOS of explosion
                             * center */
                            for (i = 0, k = 0; i < 8; i++)
                            {
                                int yy = y + ddy_ddd[i];
                                int xx = x + ddx_ddd[i];

                                /* Stay within dungeon */
                                if (!in_bounds(yy, xx))
                                    continue;

                                if (los(y2, x2, yy, xx))
                                {
                                    k++;
                                    break;
                                }
                            }

                            /* Require at least one adjacent grid in LOS */
                            if (!k)
                                continue;
                        }

                        /* We can't affect this non-passable wall */
                        else
                            continue;
                    }
                }

                /* Must be within maximum distance. */
                dist = (distance(y2, x2, y, x));
                if (dist > rad)
                    continue;

                /* Projection is a starburst */
                if (flg & (PROJECT_STAR))
                {
                    /* Grid is within effect range */
                    if (cave_info[y][x] & (CAVE_TEMP))
                    {
                        /* Reorient current grid for table access. */
                        int ny = y - y2 + 20;
                        int nx = x - x2 + 20;

                        /* Illegal table access is bad. */
                        if ((ny < 0) || (ny > 40) || (nx < 0) || (nx > 40))
                            continue;

                        /* Get angle to current grid. */
                        degree = get_angle_to_grid[ny][nx];

                        /* Scan arcs to find the one that applies here. */
                        for (i = arc_num - 1; i >= 0; i--)
                        {
                            if (arc_first[i] <= degree)
                            {
                                max_dist = arc_dist[i];

                                /* Must be within effect range. */
                                if (max_dist >= dist)
                                {
                                    gy[grids] = y;
                                    gx[grids] = x;
                                    gd[grids] = 0;
                                    grids++;
                                }

                                /* Arc found.  End search */
                                break;
                            }
                        }
                    }
                }

                /* Use angle comparison to delineate an arc. */
                else if (flg & (PROJECT_ARC))
                {
                    int n2y, n2x, tmp, diff;

                    /* Reorient current grid for table access. */
                    n2y = y - y2 + 20;
                    n2x = x - x2 + 20;

                    /*
                     * Find the angular difference (/2) between
                     * the lines to the end of the arc's center-
                     * line and to the current grid.
                     */
                    tmp = ABS(get_angle_to_grid[n2y][n2x] + centerline) % 180;
                    diff = ABS(90 - tmp);

                    /*
                     * If difference is not greater then that
                     * allowed, and the grid is in LOS, accept it.
                     */
                    if (diff < (degrees + 6) / 4)
                    {
                        if (los(y2, x2, y, x))
                        {
                            gy[grids] = y;
                            gx[grids] = x;
                            gd[grids] = dist;
                            grids++;
                        }
                    }
                }

                /* Standard ball spell -- accept all grids in LOS. */
                else
                {
                    if (flg & (PROJECT_PASS) || los(y2, x2, y, x))
                    {
                        gy[grids] = y;
                        gx[grids] = x;
                        gd[grids] = dist;
                        grids++;
                    }
                }
            }
        }
    }

    /* Clear the "temp" array  XXX */
    clear_temp_array();

    /* Calculate and store the actual damage at each distance. */
    for (i = 0; i <= MAX_RANGE; i++)
    {
        /* No damage outside the radius. */
        if (i > rad)
            dam_temp = 0;

        /* No damage reduction with range if uniform. */
        else if (uniform)
        {
            dam_temp = dd;
        }

        /* Otherwise, lose two dice per square. */
        else
        {
            if (dd > 2 * i)
                dam_temp = dd - 2 * i;
            else
                dam_temp = 0;
        }

        /* Store it. */
        dam_at_dist[i] = dam_temp;
    }

    /* Sort the blast grids by distance, starting at the origin. */
    for (i = 0, k = 0; i < rad; i++)
    {
        int tmp_y, tmp_x, tmp_d;

        /* Collect all the grids of a given distance together. */
        for (j = k; j < grids; j++)
        {
            if (gd[j] == i)
            {
                tmp_y = gy[k];
                tmp_x = gx[k];
                tmp_d = gd[k];

                gy[k] = gy[j];
                gx[k] = gx[j];
                gd[k] = gd[j];

                gy[j] = tmp_y;
                gx[j] = tmp_x;
                gd[j] = tmp_d;

                /* Write to next slot */
                k++;
            }
        }
    }

    /* Display the "blast area" if allowed */
    if (!blind && !(flg & (PROJECT_HIDE)))
    {
        /* Do the blast from inside out */
        for (i = 0; i < grids; i++)
        {
            /* Extract the location */
            y = gy[i];
            x = gx[i];

            /* Only do visuals if the player can "see" the blast */
            if (panel_contains(y, x) && player_has_los_bold(y, x))
            {
                u16b p;

                byte a;
                char c;

                drawn = true;

                /* Obtain the explosion pict */
                p = bolt_pict(y, x, y, x, typ);

                /* Extract attr/char */
                a = PICT_A(p);
                c = PICT_C(p);

                /* Present the semantic blast animation frame. */
                (void)a;
                (void)c;
            }

            /* Hack -- center the cursor */
            dungeon_note_cursor_relative(y2, x2);

            /* New radius is about to be drawn */
            if ((i == grids - 1) || ((i < grids - 1) && (gd[i + 1] > gd[i])))
            {
                /* Flush each radius separately */
                if (op_ptr->delay_factor)
                    platform_frame_present();

                /* Delay (efficiently) */
                if (visual || drawn)
                {
                    platform_frame_delay_ms((u32b)msec);
                }
            }
        }

        /* Delay for a while if there are pretty graphics to show */
        if ((grids > 1) && (visual || drawn))
        {
            if (!op_ptr->delay_factor)
                platform_frame_present();
            platform_frame_delay_ms((u32b)(50 + msec));
        }

        /* Flush the erasing -- except if we specify lingering graphics */
        if ((drawn) && (!(flg & (PROJECT_NO_REDRAW))))
        {
            /* Erase the explosion drawn above */
            for (i = 0; i < grids; i++)
            {
                /* Extract the location */
                y = gy[i];
                x = gx[i];

                /* Hack -- Erase if needed */
                if (panel_contains(y, x) && player_has_los_bold(y, x))
                {
                    dungeon_mark_map_for_redraw();
                }
            }

            /* Hack -- center the cursor */
            dungeon_note_cursor_relative(y2, x2);

            /* Flush the explosion */
            if (op_ptr->delay_factor)
                platform_frame_present();
        }
    }

    /* Check features */
    if (flg & (PROJECT_GRID))
    {
        /* Scan for features */
        for (i = 0; i < grids; i++)
        {
            /* Get the grid location */
            y = gy[i];
            x = gx[i];

            /* Affect the feature in that grid */
            if (project_f(who, y, x, gd[i], dam_at_dist[gd[i]], ds, dif, typ))
                notice = true;
        }
    }

    /* Check objects */
    if (flg & (PROJECT_ITEM))
    {
        /* Scan for objects */
        for (i = 0; i < grids; i++)
        {
            /* Get the grid location */
            y = gy[i];
            x = gx[i];

            /* Affect the object in the grid */
            if (project_o(who, y, x, dam_at_dist[gd[i]], ds, dif, typ))
                notice = true;
        }
    }

    /* Check monsters */
    if (flg & (PROJECT_KILL))
    {
        /* Mega-Hack */
        project_m_n = 0;
        project_m_x = 0;
        project_m_y = 0;
        death_count = 0;

        /* Scan for monsters */
        for (i = 0; i < grids; i++)
        {
            /* Get the grid location */
            y = gy[i];
            x = gx[i];

            /* Affect the monster in the grid */
            if (project_m(who, y, x, dam_at_dist[gd[i]], ds, dif, typ, flg))
                notice = true;
        }

        /* Player affected one monster (without "jumping") */
        if ((who < 0) && (project_m_n == 1) && !(flg & (PROJECT_JUMP)))
        {
            /* Location */
            x = project_m_x;
            y = project_m_y;

            /* Track if possible */
            if (cave_m_idx[y][x] > 0)
            {
                monster_type* m_ptr = &mon_list[cave_m_idx[y][x]];

                /* Hack -- auto-recall */
                if (m_ptr->ml)
                    monster_race_track(m_ptr->r_idx);

                /* Hack - auto-track */
                // Sil-y: turned this off experimentally
                // if (m_ptr->ml) health_track(cave_m_idx[y][x]);
            }
        }

        /* Hack -- Moria-style death messages for non-visible monsters */
        if (death_count)
        {
            /* One monster */
            if (death_count == 1)
            {
                msg_print("You hear a scream of agony!");
            }

            /* Several monsters */
            else
            {
                msg_print("You hear several screams of agony!");
            }

            /* Reset */
            death_count = 0;
        }
    }

    /* Check player */
    if (flg & (PROJECT_PLAY))
    {
        /* Scan for player */
        for (i = 0; i < grids; i++)
        {
            /* Get the grid location */
            y = gy[i];
            x = gx[i];

            /* Player is in this grid */
            if (cave_m_idx[y][x] < 0)
            {
                /* Affect the player */
                if (project_p(who, y, x, dam_at_dist[gd[i]], ds, dif, typ))
                {
                    notice = true;

                    /* Only affect the player once */
                    break;
                }
            }
        }
    }

    /* Clear the "temp" array  (paranoia is good) */
    clear_temp_array();

    /* Update stuff if needed */
    if (p_ptr->update)
        update_stuff();

    dungeon_sync_cursor_state();

    /* Return "something was noticed" */
    return (notice);
}

/************************************************************************
 *                                                                      *
 *                           Projection types                           *
 *                                                                      *
 ************************************************************************/

/*
 * Handle bolt spells.
 *
 * Bolts stop as soon as they hit a monster, whiz past missed targets, and
 * (almost) never affect items on the floor.
 */
bool project_bolt(int who, int rad, int y0, int x0, int y1, int x1, int dd,
    int ds, int dif, int typ, u32b flg)
{
    /* Add the bolt bitflags */
    flg |= PROJECT_STOP | PROJECT_KILL | PROJECT_THRU;

    /* Hurt the character unless he controls the spell */
    if (who != -1)
        flg |= PROJECT_PLAY;

    /* Limit range */
    if ((rad > MAX_RANGE) || (rad <= 0))
        rad = MAX_RANGE;

    /* Cast a bolt */
    return (project(who, rad, y0, x0, y1, x1, dd, ds, dif, typ, flg, 0, false));
}

/*
 * Handle beam spells.
 *
 * Beams affect every grid they touch, go right through monsters, and
 * (almost) never affect items on the floor.
 */
bool project_beam(int who, int rad, int y0, int x0, int y1, int x1, int dd,
    int ds, int dif, int typ, u32b flg)
{
    /* Add the beam bitflags */
    flg |= PROJECT_BEAM | PROJECT_KILL | PROJECT_THRU;

    /* Hurt the character unless he controls the spell */
    if (who != -1)
        flg |= (PROJECT_PLAY);

    /* Limit range */
    if ((rad > MAX_RANGE) || (rad <= 0))
        rad = MAX_RANGE;

    /* Cast a beam */
    return (project(who, rad, y0, x0, y1, x1, dd, ds, dif, typ, flg, 0, false));
}

/*
 * Handle ball spells.
 *
 * Balls act like bolt spells, except that they do not pass their target,
 * and explode when they hit a monster, a wall, their target, or the edge
 * of sight.  Within the explosion radius, they affect items on the floor.
 *
 * Balls may jump to the target, and have any source diameter (which affects
 * how quickly their damage falls off with distance from the center of the
 * explosion).
 */
bool project_ball(int who, int rad, int y0, int x0, int y1, int x1, int dd,
    int ds, int dif, int typ, u32b flg, bool uniform)
{
    /* Add the ball bitflags */
    flg |= PROJECT_BOOM | PROJECT_GRID | PROJECT_ITEM | PROJECT_KILL;

    /* Add the STOP flag if appropriate */
    if ((who < 0)
        && (!target_okay(0) || y1 != p_ptr->target_row
            || x1 != p_ptr->target_col))
    {
        flg |= (PROJECT_STOP);
    }

    /* Hurt the character unless he controls the spell */
    if (who != -1)
        flg |= (PROJECT_PLAY);

    /* Limit radius to nine (up to 256 grids affected) */
    if (rad > 9)
        rad = 9;

    /* Cast a ball */
    return (
        project(who, rad, y0, x0, y1, x1, dd, ds, dif, typ, flg, 0, uniform));
}

/*
 * Handle ball spells that explode immediately on the target and
 * hurt everything.
 */
bool explosion(
    int who, int rad, int y0, int x0, int dd, int ds, int dif, int typ)
{
    /* Add the explosion bitflags */
    u32b flg = PROJECT_BOOM | PROJECT_GRID | PROJECT_JUMP | PROJECT_ITEM
        | PROJECT_KILL | PROJECT_PLAY;

    /* Explode */
    return (
        project_ball(who, rad, y0, x0, y0, x0, dd, ds, dif, typ, flg, false));
}

/*
 * Handle arc spells.
 *
 * Arcs are a pie-shaped segment (with a width determined by "degrees")
 * of a explosion outwards from the source grid.  They are centered
 * along a line extending from the source towards the target.  -LM-
 *
 * Because all arcs start out as being one grid wide, arc spells with a
 * value for degrees of arc less than (roughly) 60 do not dissipate as
 * quickly.  In the extreme case where degrees of arc is 0, the arc is
 * actually a defined length beam, and loses no strength at all over the
 * ranges found in the game.
 *
 * Arcs affect items on the floor.
 */
bool project_arc(int who, int rad, int y0, int x0, int y1, int x1, int dd,
    int ds, int dif, int typ, u32b flg, int degrees)
{
    /* Radius of zero means no fixed limit. */
    if (rad == 0)
        rad = MAX_SIGHT;

    /* If the arc has no spread, it's actually a beam */
    if (degrees <= 0)
    {
        /* Add the beam bitflags */
        flg |= (PROJECT_BEAM | PROJECT_KILL);
    }

    /* If a full circle is asked for, we cast a ball spell. */
    else if (degrees >= 360)
    {
        /* Add the ball bitflags */
        flg |= PROJECT_STOP | PROJECT_BOOM | PROJECT_GRID | PROJECT_ITEM
            | PROJECT_KILL;
    }

    /* Otherwise, we fire an arc */
    else
    {
        /* Add the arc bitflags */
        flg |= PROJECT_ARC | PROJECT_BOOM | PROJECT_GRID | PROJECT_ITEM
            | PROJECT_KILL;
    }

    /* Hurt the character unless he controls the spell */
    if (who != -1)
        flg |= (PROJECT_PLAY);

    /* Cast an arc (or a ball) */
    return (project(
        who, rad, y0, x0, y1, x1, dd, ds, dif, typ, flg, degrees, false));
}

/*
 * Handle target grids for projections under the control of
 * the character.  - Chris Wilde, Morgul
 */
static void adjust_target(int dir, int y0, int x0, int* y1, int* x1)
{
    /* If no direction is given, and a target is, use the target. */
    if ((dir == 5) && target_okay(0))
    {
        *y1 = p_ptr->target_row;
        *x1 = p_ptr->target_col;
    }
    else if ((dir == DIRECTION_UP) || (dir == DIRECTION_DOWN))
    {
        *y1 = y0;
        *x1 = x0;
    }

    /* Otherwise, use the given direction */
    else
    {
        *y1 = y0 + MAX_RANGE * ddy[dir];
        *x1 = x0 + MAX_RANGE * ddx[dir];
    }
}

/*
 * Apply a "project()" directly to all monsters in view of a certain spot.
 *
 * Note that affected monsters are NOT auto-tracked by this usage.
 *
 * This function is not optimized for efficieny.  It should only be used
 * in non-bottleneck functions such as spells. It should not be used in
 * functions that are major code bottlenecks such as process monster or
 * update_view. -JG
 */
bool project_los_not_player(int y1, int x1, int dd, int ds, int dif, int typ)
{
    int i, x, y;

    u32b flg = PROJECT_JUMP | PROJECT_KILL | PROJECT_HIDE;

    bool obvious = false;

    /* Affect all (nearby) monsters */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];

        /* Paranoia -- Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Location */
        y = m_ptr->fy;
        x = m_ptr->fx;

        /*The LOS function doesn't do well with long distances*/
        if (distance(y1, x1, y, x) > MAX_RANGE)
            continue;

        /* Require line of sight or the monster being right on the square */
        if ((y != y1) || (x != x1))
        {
            if (!los(y1, x1, y, x))
                continue;
        }

        /* Jump directly to the target monster */
        if (project(-1, 0, y, x, y, x, dd, ds, dif, typ, flg, 0, false))
            obvious = true;
    }

    /* Result */
    return (obvious);
}

/*
 * Apply a "project()" directly to all viewable monsters
 *
 * Note that affected monsters are NOT auto-tracked by this usage.
 */
bool project_los(int typ, int dd, int ds, int dif, bool silent)
{
    int i, x, y;

    u32b flg = PROJECT_JUMP | PROJECT_KILL | PROJECT_HIDE;
    if (silent)
    {
        flg |= PROJECT_SILENT;
    }

    bool obvious = false;

    /* Affect all (nearby) monsters */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];

        /* Paranoia -- Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Location */
        y = m_ptr->fy;
        x = m_ptr->fx;

        /* Require line of fire */
        if (!player_can_fire_bold(y, x))
            continue;
        if (!player_has_los_bold(y, x))
            continue;

        /* Jump directly to the target monster */
        if (project(-1, 0, y, x, y, x, dd, ds, dif, typ, flg, 0, false))
            obvious = true;
    }

    /* Result */
    return (obvious);
}

/*
 * Apply a "project()" directly to all viewable grids
 */
bool project_los_grids(int typ, int dd, int ds, int dif)
{
    int x, y;
    u32b flg = PROJECT_GRID | PROJECT_ITEM | PROJECT_HIDE | PROJECT_JUMP;

    bool obvious = false;

    for (y = p_ptr->py - MAX_SIGHT; y < p_ptr->py + MAX_SIGHT; y++)
    {
        for (x = p_ptr->px - MAX_SIGHT; x < p_ptr->px + MAX_SIGHT; x++)
        {
            if (!in_bounds_fully(y, x))
                continue;

            if (!player_has_los_bold(y, x))
                continue;

            if (project(-1, 0, y, x, y, x, dd, ds, dif, typ, flg, 0, false))
            {
                obvious = true;
            }
        }
    }
    /* Result */
    return (obvious);
}

/*
 * This routine clears the entire "temp" set.
 */
void clear_temp_array(void)
{
    int i;

    /* Apply flag changes */
    for (i = 0; i < temp_n; i++)
    {
        int y = temp_y[i];
        int x = temp_x[i];

        /* No longer in the array */
        cave_info[y][x] &= ~(CAVE_TEMP);
    }

    /* None left */
    temp_n = 0;
}

/*
 * Aux function -- see below
 */
void cave_temp_mark(int y, int x, bool room)
{
    /* Avoid infinite recursion */
    if (cave_info[y][x] & (CAVE_TEMP))
        return;

    /* Option -- do not leave the current room */
    if ((room) && (!(cave_info[y][x] & (CAVE_ROOM))))
        return;

    /* Verify space */
    if (temp_n == TEMP_MAX)
        return;

    /* Mark the grid */
    cave_info[y][x] |= (CAVE_TEMP);

    /* Add it to the marked set */
    temp_y[temp_n] = y;
    temp_x[temp_n] = x;
    temp_n++;
}

/*
 * Mark the nearby area with CAVE_TEMP flags.  Allow limited range.
 */
void spread_cave_temp(int y1, int x1, int range, bool room)
{
    int i, y, x;

    /* Add the initial grid */
    cave_temp_mark(y1, x1, room);

    /* While grids are in the queue, add their neighbors */
    for (i = 0; i < temp_n; i++)
    {
        x = temp_x[i], y = temp_y[i];

        /* Walls get marked, but stop further spread */
        if (!cave_floor_bold(y, x))
            continue;

        /* Note limited range (note:  we spread out one grid further) */
        if ((range) && (distance(y1, x1, y, x) >= range))
            continue;

        /* Spread adjacent */
        cave_temp_mark(y + 1, x, room);
        cave_temp_mark(y - 1, x, room);
        cave_temp_mark(y, x + 1, room);
        cave_temp_mark(y, x - 1, room);

        /* Spread diagonal */
        cave_temp_mark(y + 1, x + 1, room);
        cave_temp_mark(y - 1, x - 1, room);
        cave_temp_mark(y - 1, x + 1, room);
        cave_temp_mark(y + 1, x - 1, room);
    }
}

/*
 * Slow monsters
 */
bool slow_monsters(int power) { return (project_los(GF_SLOW, 0, 0, power, false)); }

/*
 * Sleep monsters
 */
bool sleep_monsters(int power) { return (project_los(GF_SLEEP, 0, 0, power, false)); }


/*
 * Character casts a special-purpose bolt or beam spell.
 */
bool fire_bolt_beam_special(
    int typ, int dir, int dd, int ds, int dif, int rad, u32b flg)
{
    int y1, x1;

    /* Get target */
    adjust_target(dir, p_ptr->py, p_ptr->px, &y1, &x1);

    /* This is a beam spell */
    if (flg & (PROJECT_BEAM))
    {
        /* Cast a beam */
        return (project_beam(
            -1, rad, p_ptr->py, p_ptr->px, y1, x1, dd, ds, dif, typ, flg));
    }

    /* This is a bolt spell */
    else
    {
        /* Cast a bolt */
        return (project_bolt(
            -1, rad, p_ptr->py, p_ptr->px, y1, x1, dd, ds, dif, typ, flg));
    }
}

/*
 * Character casts a (simple) ball spell.
 */
bool fire_ball(int typ, int dir, int dd, int ds, int dif, int rad)
{
    int y1, x1;

    /* Get target */
    adjust_target(dir, p_ptr->py, p_ptr->px, &y1, &x1);

    /* Cast a (simple) ball */
    return (project_ball(
        -1, rad, p_ptr->py, p_ptr->px, y1, x1, dd, ds, dif, typ, 0L, false));
}

/*
 * Character casts an arc spell.
 */
bool fire_arc(int typ, int dir, int dd, int ds, int dif, int rad, int degrees)
{
    int y1, x1;

    /* Get target */
    adjust_target(dir, p_ptr->py, p_ptr->px, &y1, &x1);

    /* Cast an arc */
    return (project_arc(
        -1, rad, p_ptr->py, p_ptr->px, y1, x1, dd, ds, dif, typ, 0L, degrees));
}

/*
 * Character casts a bolt spell.
 */
bool fire_bolt(int typ, int dir, int dd, int ds, int dif)
{
    int y1, x1;

    /* Get target */
    adjust_target(dir, p_ptr->py, p_ptr->px, &y1, &x1);

    /* Cast a bolt */
    return (project_bolt(
        -1, MAX_RANGE, p_ptr->py, p_ptr->px, y1, x1, dd, ds, dif, typ, 0L));
}

/*
 * Character casts a beam spell.
 */
bool fire_beam(int typ, int dir, int dd, int ds, int dif)
{
    int y1, x1;

    /* Get target */
    adjust_target(dir, p_ptr->py, p_ptr->px, &y1, &x1);

    /* Cast a beam */
    return (project_beam(
        -1, MAX_RANGE, p_ptr->py, p_ptr->px, y1, x1, dd, ds, dif, typ, 0L));
}

/*
 * Cast a bolt or a beam spell
 */
bool fire_bolt_or_beam(int prob, int typ, int dir, int dd, int ds, int dif)
{
    if (percent_chance(prob))
    {
        return (fire_beam(typ, dir, dd, ds, dif));
    }
    else
    {
        return (fire_bolt(typ, dir, dd, ds, dif));
    }
}

/*
 * Some of the old functions
 */

bool light_line(int dir)
{
    u32b flg = PROJECT_BEAM | PROJECT_GRID;
    return (fire_bolt_beam_special(GF_LIGHT, dir, 6, 4, -1, MAX_RANGE, flg));
}

bool destroy_door(int dir)
{
    u32b flg = PROJECT_BEAM | PROJECT_GRID | PROJECT_ITEM;
    return (
        fire_bolt_beam_special(GF_KILL_DOOR, dir, 0, 0, -1, MAX_RANGE, flg));
}

bool disarm_trap(int dir)
{
    u32b flg = PROJECT_BEAM | PROJECT_GRID | PROJECT_ITEM;
    return (
        fire_bolt_beam_special(GF_KILL_TRAP, dir, 0, 0, -1, MAX_RANGE, flg));
}


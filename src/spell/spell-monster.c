/* File: spell/spell-monster.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

/*
 * Monster control spells and monster-facing song effects.
 * Split from spells1.c and spells2.c for better code organization.
 */

#include "angband.h"
#include "player/killer.h"
#include "player/player-songs.h"
#include "spell/spell-monster.h"

/*
 *  Do the effects of (the monster song) Song of Binding
 */
void song_of_binding(monster_type* m_ptr)
{
    int y, x;
    int resistance;
    int result;
    int dist = flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx);
    char m_name[80];
    cptr description;

    int song_skill = r_info[m_ptr->r_idx].wil; // Use Will as song skill

    if (m_ptr->song_lockout_timer > 0 && m_ptr->song != SNG_BINDING)
        return;

    /* Get the monster name */
    monster_desc(m_name, sizeof(m_name), m_ptr, 0x80);

    // messages for beginning a new song
    if (m_ptr->song != SNG_BINDING)
    {
        msg_format("%^s begins a song of binding.", m_name);

        // and remember the monsters is now singing this song
        m_ptr->song = SNG_BINDING;

        // disturb if message printed
        disturb(1, 0);
    }

    // messages for continuing a song
    else
    {
        switch (dieroll(8))
        {
        case 1:
            description = "durance";
            break;
        case 2:
            description = "chains";
            break;
        case 3:
            description = "thralls";
            break;
        case 4:
            description = "prison walls";
            break;
        case 5:
            description = "locks without keys";
            break;
        default:
            description = "binding";
        }

        if (m_ptr->ml)
            msg_format("%^s sings of %s.", m_name, description);
        else if (dist <= 20)
            msg_format("You hear a song of %s.", description);
        else if (dist <= 30)
            msg_print("You hear singing in the distance.");

        // disturb if message printed
        if (m_ptr->ml || (dist <= 30))
            disturb(1, 0);
    }

    // use the monster noise flow to represent the song levels at each square
    update_flow(m_ptr->fy, m_ptr->fx, FLOW_MONSTER_NOISE);

    // Morgoth's throne-room fight should not revolve around him repeatedly
    // re-closing the hall's doors, but keep the song's slowing effect.
    if (!((m_ptr->r_idx == R_IDX_MORGOTH)
            && (p_ptr->depth == MORGOTH_DEPTH)
            && p_ptr->morgoth_hall_entered
            && !p_ptr->on_the_run))
    {
        // scan the map, closing doors
        for (y = 0; y < p_ptr->cur_map_hgt; y++)
        {
            for (x = 0; x < p_ptr->cur_map_wid; x++)
            {
                if (!in_bounds_fully(y, x))
                    continue;

                // if there is no player/monster in the square
                if (cave_m_idx[y][x] == 0)
                {
                    // if it is a door
                    if ((cave_feat[y][x] == FEAT_OPEN)
                        || (cave_feat[y][x] == FEAT_BROKEN)
                        || cave_known_closed_door_bold(y, x))
                    {
                        // if the door isn't between the monster and the player
                        if (!(ORDERED(m_ptr->fy, y, p_ptr->py)
                                && ORDERED(m_ptr->fx, x, p_ptr->px)))
                        {
                            result = skill_check(m_ptr, song_skill,
                                15 + flow_dist(FLOW_MONSTER_NOISE, y, x), NULL);

                            (void)lock_door(y, x, result);
                        }
                    }
                }
            }
        }
    }

    /*
    // scan the map, slowing monsters
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            if (!in_bounds_fully(y, x)) continue;

            // if there is a monster in the square
            if ((cave_m_idx[y][x] > 0) && !((y == m_ptr->fy) && (x ==
    m_ptr->fx)))
            {
                monster_type *n_ptr = &mon_list[cave_m_idx[y][x]];

                resistance = monster_skill(n_ptr, S_WIL) + 5 +
    flow_dist(FLOW_MONSTER_NOISE, y, x);

                result = skill_check(m_ptr, song_skill, resistance, n_ptr);

                // if the check succeeds, the monster is slowed for at least 2
    rounds if (result > 0)
                {
                    set_monster_slow(cave_m_idx[y][x], MAX(m_ptr->slowed, 2),
    mon_list[cave_m_idx[y][x]].ml);
                }
            }
        }
    }
    */

    // if the player is singing the song of silence, then the  monster suffers a
    // penalty
    if (singing(SNG_SILENCE))
        song_skill -= ability_bonus(S_SNG, SNG_SILENCE) / 2;

    // determine the player's resistance
    // Sil-y: might want to add in the same +5 bonus as against Mastery and
    // Lorien
    resistance = p_ptr->skill_use[S_WIL] + (p_ptr->free_act * 10)
        + flow_dist(FLOW_MONSTER_NOISE, p_ptr->py, p_ptr->px);

    // Sil-y: ideally we'd use a call to allow_player_slow() here, but that
    // doesn't
    //        work as it can't take the noise distance into account.
    //        Sadly my solution doesn't let you ID free action items.
    result = skill_check(m_ptr, song_skill, resistance, PLAYER);

    // if the check succeeds, the player is slowed for at least 2 rounds
    // note that only the first of these affects you as you aren't slow on the
    // round it wears off
    if (result > 0)
    {
        set_slow(MAX(p_ptr->slow, 2));
    }
}

/*
 *  Do the effects of (the monster song) Song of Piercing
 */
void song_of_piercing(monster_type* m_ptr)
{
    int resistance;
    int result;
    int dist = flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx);
    char m_name[80];
    cptr description;

    int song_skill = r_info[m_ptr->r_idx].wil; // Use Will as song skill

    if (m_ptr->song_lockout_timer > 0 && m_ptr->song != SNG_PIERCING)
        return;

    /* Get the monster name */
    monster_desc(m_name, sizeof(m_name), m_ptr, 0x80);

    // messages for beginning a new song
    if ((m_ptr->song != SNG_PIERCING) && m_ptr->ml)
    {
        msg_format("%^s begins a song of piercing.", m_name);

        // and remember the monsters is now singing this song
        m_ptr->song = SNG_PIERCING;

        // disturb if message printed
        disturb(1, 0);
    }

    // messages for continuing a song
    else
    {
        switch (dieroll(8))
        {
        case 1:
            description = "opening";
            break;
        case 2:
            description = "treachery";
            break;
        case 3:
            description = "revealing";
            break;
        case 4:
            description = "uncovering";
            break;
        case 5:
            description = "betraying";
            break;
        default:
            description = "piercing";
        }

        if (m_ptr->ml)
            msg_format("%^s sings of %s.", m_name, description);
        else if (dist <= 20)
            msg_format("You hear a song of %s.", description);
        else if (dist <= 30)
            msg_print("You hear singing in the distance.");

        // disturb if message printed
        if (m_ptr->ml || (dist <= 30))
            disturb(1, 0);
    }

    // if the player is singing the song of silence, then the  monster suffers a
    // penalty
    if (singing(SNG_SILENCE))
        song_skill -= ability_bonus(S_SNG, SNG_SILENCE) / 2;

    // determine the player's resistance
    resistance = p_ptr->skill_use[S_WIL] + dist + 5;

    // perform the skill check
    result = skill_check(m_ptr, song_skill, resistance, PLAYER);

    // if the check succeeds, Morgoth knows the player's location
    if (result > 0)
    {
        msg_print("You feel your mind laid bare before Morgoth's will.");
        set_alertness(m_ptr, MIN(result, ALERTNESS_VERY_ALERT));
        /* Give Morgoth a fresh pursuit target after a successful piercing. */
        m_ptr->target_y = p_ptr->py;
        m_ptr->target_x = p_ptr->px;
    }

    else if (result > -5)
    {
        msg_print(
            "You feel the force of Morgoth's will searching for the intruder.");
    }
}

/*
 *  Do the effects of (the monster song) Song of Oaths
 */
void song_of_oaths(monster_type* m_ptr)
{
    int y, x;
    int result;
    int range;
    int dist = flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx);
    char m_name[80];
    cptr description;

    int song_skill = 21; // Gorthaur's song skill. If more monsters get songs
                         // I'll put this in monster.txt

    if (m_ptr->song_lockout_timer > 0 && m_ptr->song != SNG_OATHS)
        return;

    /* Get the monster name */
    monster_desc(m_name, sizeof(m_name), m_ptr, 0x80);

    // messages for beginning a new song
    if (m_ptr->song != SNG_OATHS)
    {
        msg_format("%^s begins a song of oaths.", m_name);

        // and remember the monsters is now singing this song
        m_ptr->song = SNG_OATHS;

        // disturb if message printed
        disturb(1, 0);
    }

    // messages for continuing a song
    else
    {
        switch (dieroll(8))
        {
        case 1:
            description = "vows broken";
            break;
        case 2:
            description = "promises";
            break;
        case 3:
            description = "duty";
            break;
        case 4:
            description = "tasks forgotten";
            break;
        case 5:
            description = "redemption";
            break;
        default:
            description = "oaths";
        }

        if (m_ptr->ml)
            msg_format("%^s sings of %s.", m_name, description);
        else if (dist <= 20)
            msg_format("You hear a song of %s.", description);
        else if (dist <= 30)
            msg_print("You hear singing in the distance.");

        // Disturb if message printed
        if (m_ptr->ml || (dist <= 30))
            disturb(1, 0);
    }

    // use the monster noise flow to represent the song levels at each square
    update_flow(m_ptr->fy, m_ptr->fx, FLOW_MONSTER_NOISE);

    // if the player is singing the song of silence, then the  monster suffers a
    // penalty
    if (singing(SNG_SILENCE))
        song_skill -= ability_bonus(S_SNG, SNG_SILENCE) / 2;

    // perform the skill check
    result = skill_check(m_ptr, song_skill, 15, PLAYER);

    // if the check was successful, summon an oathwraith to a nearby square
    if (result > 0)
    {
        int attempts = 10;

        // the greatest distance away the wraith can be summoned -- smaller is
        // typically better
        range = MAX(15 - result, 3);

        while (attempts--)
        {
            // choose a random square
            y = rand_int(p_ptr->cur_map_hgt);
            x = rand_int(p_ptr->cur_map_wid);

            if (!in_bounds(y, x))
                continue;

            // check the square is empty and close enough
            if (cave_empty_bold(y, x)
                && flow_dist(FLOW_MONSTER_NOISE, y, x) <= range)
            {
                monster_type* n_ptr;

                // place it
                place_monster_one(y, x, R_IDX_OATHWRAITH, true, false, NULL);

                n_ptr = &mon_list[cave_m_idx[y][x]];

                // message if visible
                if (n_ptr->ml)
                    msg_print("An Oathwraith appears.");

                // mark the wraith as having been summoned
                n_ptr->mflag |= (MFLAG_SUMMONED);

                // let it know where the player is
                set_alertness(n_ptr, ALERTNESS_QUITE_ALERT);

                break;
            }
        }
    }
}

void hatch_spider(monster_type* m_ptr)
{
    char m_name[80];

    /* Get the monster name */
    monster_desc(m_name, sizeof(m_name), m_ptr, 0x80);

    if (m_ptr->ml)
        msg_format("An egg on %s's back hatches.", m_name);
    reproduce_monster(cave_m_idx[m_ptr->fy][m_ptr->fx], R_IDX_SPIDER_HATCHLING);

    // Monster still gets to attack next turn
    m_ptr->energy += 50;
}

/*
 * Destroy traps
 */
bool destroy_traps(int power)
{
    return (project_los_grids(GF_KILL_TRAP, 0, 0, power));
}

/*
 * Open doors
 */
bool open_doors(int power)
{
    return (project_los_grids(GF_KILL_DOOR, 0, 0, power));
}

/*
 * Close and lock doors
 */
bool lock_doors(int power)
{
    return (project_los_grids(GF_LOCK_DOOR, 0, 0, power));
}

/*
 * Wake up all monsters, and speed up "los" monsters.
 */
void wake_all_monsters(int who)
{
    int i;

    /* Aggravate everyone */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];
        /* Paranoia -- Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Skip aggravating monster (or player) */
        if (i == who)
            continue;

        // Alert it
        set_alertness(m_ptr, MAX(m_ptr->alertness, ALERTNESS_VERY_ALERT));

        /*possibly update the monster health bar*/
        if (p_ptr->health_who == i)
            p_ptr->redraw |= (PR_HEALTHBAR);
    }
}

/*
 * Set the aggressive flag on nearby monsters (using the sound metric).
 */
bool make_aggressive(void)
{
    int i;
    int notice = false;

    for (i = 1; i < mon_max; i++)
    {
        /* Check the i'th monster */
        monster_type* m_ptr = &mon_list[i];
        monster_race* r_ptr = &r_info[m_ptr->r_idx];

        if ((m_ptr->alertness >= ALERTNESS_ALERT)
            && (flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx) <= 10))
        {
            m_ptr->mflag |= (MFLAG_AGGRESSIVE);

            // notice if the monster is visible
            if (m_ptr->ml)
                notice = true;

            if ((r_ptr->flags2 & (RF2_SMART))
                && ((r_ptr->flags1 & (RF1_FRIENDS))
                    || (r_ptr->flags1 & (RF1_FRIEND))
                    || (r_ptr->flags1 & (RF1_UNIQUE_FRIEND))
                    || (r_ptr->flags1 & (RF1_ESCORT))
                    || (r_ptr->flags1 & (RF1_ESCORTS))
                    || (r_ptr->flags4 & (RF4_SHRIEK))))
            {
                tell_allies(m_ptr->fy, m_ptr->fx, MFLAG_AGGRESSIVE);

                // notice if you hear them shout
                notice = true;
            }
        }
    }

    return (notice);
}

/*
 * Delete all non-unique monsters of a given "type" from the level
 */
bool banishment(void)
{
    int i;

    char typ;

    /* Mega-Hack -- Get a monster symbol */
    if (!get_com("Choose a monster race (by symbol) to banish: ", &typ))
        return false;

    /* Delete the monsters of that "type" */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];
        monster_race* r_ptr = &r_info[m_ptr->r_idx];

        /* Paranoia -- Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Hack -- Skip Unique Monsters */
        if (r_ptr->flags1 & (RF1_UNIQUE))
            continue;

        /* Skip "wrong" monsters */
        if (r_ptr->d_char != typ)
            continue;

        /* Delete the monster */
        delete_monster_idx(i);

        /* Take some damage */
        killer_mark_other(SCORE_KILLER_SELF);
        take_hit(dieroll(4), "the strain of casting Banishment");
    }

    /* Success */
    return true;
}

/*
 * Delete all nearby (non-unique) monsters
 */
bool mass_banishment(void)
{
    int i;

    bool result = false;

    /* Delete the (nearby) monsters */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];
        monster_race* r_ptr = &r_info[m_ptr->r_idx];

        /* Paranoia -- Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Hack -- Skip unique monsters */
        if (r_ptr->flags1 & (RF1_UNIQUE))
            continue;

        /* Skip distant monsters */
        if (m_ptr->cdis > MAX_SIGHT)
            continue;

        /* Delete the monster */
        delete_monster_idx(i);

        /* Take some damage */
        killer_mark_other(SCORE_KILLER_SELF);
        take_hit(dieroll(3), "the strain of casting Mass Banishment");

        /* Note effect */
        result = true;
    }

    return (result);
}


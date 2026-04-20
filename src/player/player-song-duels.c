/* File: player/player-song-duels.c */
/*
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

#include "angband.h"
#include "player/player-song-duels.h"

#define SONG_DUEL_STACK_LIMIT 3
#define SONG_DUEL_LOCKOUT_TURNS 10

bool song_is_duel(int song)
{
    return (song == SNG_CONTEST || song == SNG_LAMENT);
}

void display_synergy_message(int song1, int song2)
{
    // Check if we have a valid synergy pair
    if (song1 == SNG_NOTHING || song2 == SNG_NOTHING)
        return;

    // Define synergy pairs and their messages
    struct {
        int song_a;
        int song_b;
        const char* message;
    } synergies[] = {
        { SNG_ELBERETH, SNG_TREES,
          "The starlight and the Two Trees harmonize in glorious unity!" },
        { SNG_ELBERETH, SNG_STAUNCHING,
          "Starlight and healing blend into a restorative radiance!" },
        { SNG_CHALLENGE, SNG_SLAYING,
          "Your fury and mockery blend into a devastating war-song!" },
        { SNG_DELVINGS, SNG_REVEALING,
          "Stone and secrets resonate together, unveiling all that is hidden!" },
        { SNG_FREEDOM, SNG_ELVENESS,
          "Grace and liberty intertwine in an uplifting melody!" },
        { SNG_STAYING, SNG_CONTEST,
          "Your courage strengthens your voice in the duel of songs!" },
        { SNG_STAYING, SNG_LAMENT,
          "Courage and sorrow unite in a song of enduring strength!" },
        { SNG_SILENCE, SNG_DISGUISE,
          "Quietness and guile weave a cloak of perfect concealment!" },
        { SNG_SILENCE, SNG_LORIEN,
          "Silence and rest deepen into profound tranquility!" },
        { SNG_SHATTERING, SNG_MASTERY,
          "Destruction and dominion unite in overwhelming force!" }
    };

    for (size_t i = 0; i < sizeof(synergies) / sizeof(synergies[0]); i++)
    {
        if ((song1 == synergies[i].song_a && song2 == synergies[i].song_b)
            || (song1 == synergies[i].song_b && song2 == synergies[i].song_a))
        {
            msg_print(synergies[i].message);
            return;
        }
    }
}

void song_duel_clear_player_target(void)
{
    p_ptr->song_target_idx = 0;
    p_ptr->song_target_song = SNG_NOTHING;
}

static monster_type* song_duel_get_target(int song)
{
    if (!song_is_duel(song))
        return NULL;

    if (p_ptr->song_target_song != song)
        return NULL;

    int m_idx = p_ptr->song_target_idx;
    if (m_idx <= 0 || m_idx >= mon_max)
        return NULL;

    monster_type* m_ptr = &mon_list[m_idx];
    if (!m_ptr->r_idx)
        return NULL;

    return m_ptr;
}

void song_duel_reset_player_stack(void)
{
    p_ptr->song_contest_player_stacks = 0;
    p_ptr->song_contest_last_turn = 0;
}

static void song_duel_reset_monster_stack(monster_type* m_ptr, int song)
{
    if (song == SNG_CONTEST)
    {
        m_ptr->song_contest_stacks = 0;
        m_ptr->song_contest_last_turn = 0;
    }
    else if (song == SNG_LAMENT)
    {
        m_ptr->song_lament_stacks = 0;
        m_ptr->song_lament_last_turn = 0;
    }
}

bool song_duel_select_target(int song)
{
    const char* prompt = (song == SNG_CONTEST)
        ? "Choose a foe to challenge with your contest."
        : "Choose a foe to bear the weight of your lament.";

    msg_print(prompt);

    if (!target_set_interactive(TARGET_KILL, 0))
    {
        msg_print("You let the song fade before it finds a target.");
        return false;
    }

    if (!p_ptr->target_set || p_ptr->target_who <= 0)
    {
        msg_print("You let the song fade before it finds a target.");
        return false;
    }

    monster_type* m_ptr = &mon_list[p_ptr->target_who];
    if (!m_ptr->r_idx)
    {
        msg_print("No suitable foe answers your song.");
        return false;
    }

    // Check if this monster has already completed a duel of this type
    if (song == SNG_CONTEST && m_ptr->song_contest_completed)
    {
        msg_print("You have already completed a contest with this foe.");
        return false;
    }
    else if (song == SNG_LAMENT && m_ptr->song_lament_completed)
    {
        msg_print("You have already sung your lament against this foe.");
        return false;
    }

    song_duel_clear_player_target();
    song_duel_reset_player_stack();

    p_ptr->song_target_idx = p_ptr->target_who;
    p_ptr->song_target_song = song;

    if (song == SNG_CONTEST)
    {
        m_ptr->song_contest_stacks = 0;
        m_ptr->song_contest_last_turn = playerturn;
    }
    else
    {
        m_ptr->song_lament_stacks = 0;
        m_ptr->song_lament_last_turn = playerturn;
    }

    // Wake up and alert the monster - it notices the song directed at it
    set_alertness(m_ptr, ALERTNESS_VERY_ALERT);
    update_mon(p_ptr->target_who, false);

    return true;
}

static void song_duel_reduce_monster_hp(monster_type* m_ptr, int steps)
{
    if (steps <= 0)
        return;

    int old_maxhp = m_ptr->maxhp;
    if (old_maxhp <= 0)
        old_maxhp = 1;

    int new_maxhp = old_maxhp;

    for (int i = 0; i < steps; i++)
    {
        new_maxhp = (new_maxhp * 10 + 11) / 12;
        if (new_maxhp < 1)
        {
            new_maxhp = 1;
            break;
        }
    }

    if (new_maxhp < 1)
        new_maxhp = 1;

    if (new_maxhp < m_ptr->maxhp)
    {
        long scaled = (long)m_ptr->hp * new_maxhp;
        m_ptr->hp = (int)(scaled / old_maxhp);
        if (m_ptr->hp < 1)
            m_ptr->hp = 1;
        if (m_ptr->hp > new_maxhp)
            m_ptr->hp = new_maxhp;

        int hp_loss = m_ptr->maxhp - new_maxhp;
        if (hp_loss > 0)
            monster_add_song_hp_loss(m_ptr, hp_loss);

        m_ptr->maxhp = new_maxhp;

        /* Morgoth's anger state depends on current HP% (and maxHP can change here). */
        maybe_update_morgoth_state_from_hp(m_ptr);
    }
}

static void song_duel_reduce_monster_damage_dice(monster_type* m_ptr, int penalty)
{
    if (penalty <= 0)
        return;

    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    for (int b = 0; b < MONSTER_BLOW_MAX; b++)
    {
        if (!r_ptr->blow[b].method)
            continue;

        int max_reduction = (r_ptr->blow[b].dd > 1) ? (r_ptr->blow[b].dd - 1) : 0;
        if (max_reduction <= 0)
            continue;

        int total = m_ptr->blow_dd_reduction[b] + penalty;
        if (total > max_reduction)
            total = max_reduction;
        m_ptr->blow_dd_reduction[b] = (byte)total;
    }
}

static void song_duel_apply_lament_penalties(monster_type* m_ptr, int song_skill)
{
    int will_penalty = MAX(1, song_skill / 2);
    int con_penalty = MAX(1, song_skill / 12);

    m_ptr->song_will_penalty += will_penalty;

    song_duel_reduce_monster_hp(m_ptr, con_penalty);
    song_duel_reduce_monster_damage_dice(m_ptr, con_penalty);
}

static void song_duel_apply_contest_penalties(monster_type* m_ptr, int song_skill)
{
    int will_penalty = MAX(1, song_skill / 3);
    int stealth_penalty = MAX(1, song_skill / 2);
    int evasion_penalty = MAX(1, song_skill / 5);
    int armor_penalty = MAX(1, song_skill / 12);

    m_ptr->song_will_penalty += will_penalty;
    m_ptr->song_stealth_penalty += stealth_penalty;
    m_ptr->song_evasion_penalty += evasion_penalty;

    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    int max_penalty = r_ptr->pd;

    if (armor_penalty > 0)
    {
        int total = m_ptr->song_armor_dice_penalty + armor_penalty;
        if (total > max_penalty)
            total = max_penalty;
        m_ptr->song_armor_dice_penalty = (byte)total;
    }
}

static void song_duel_finish_monster_loss(monster_type* m_ptr, int song, int song_skill)
{
    char m_name[80];
    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

    if (song == SNG_CONTEST)
        msg_format("Your contest overwhelms %s!", m_name);
    else
        msg_format("%s succumbs to your lament!", m_name);

    if (song == SNG_CONTEST)
        song_duel_apply_contest_penalties(m_ptr, song_skill);
    else
    {
        song_duel_apply_lament_penalties(m_ptr, song_skill);
        // Song of Lament always drains Grace - no resistance
        if (dec_stat(A_GRA, 1, false))
            msg_print("You feel drained.");
    }

    m_ptr->song = SNG_NOTHING;
    m_ptr->song_lockout_timer = SONG_DUEL_LOCKOUT_TURNS;

    // Mark this duel as completed so it can't be re-targeted
    if (song == SNG_CONTEST)
        m_ptr->song_contest_completed = 1;
    else
        m_ptr->song_lament_completed = 1;

    song_duel_reset_monster_stack(m_ptr, SNG_CONTEST);
    song_duel_reset_monster_stack(m_ptr, SNG_LAMENT);

    song_duel_clear_player_target();
    song_duel_reset_player_stack();

    p_ptr->song_lockout_timer = SONG_DUEL_LOCKOUT_TURNS;

    change_song(SNG_NOTHING);
}

static void song_duel_finish_player_loss(int song, monster_type* m_ptr)
{
    msg_print("You can no longer sustain the song.");

    // Mark this duel as completed so it can't be re-targeted
    if (m_ptr)
    {
        if (song == SNG_CONTEST)
            m_ptr->song_contest_completed = 1;
        else if (song == SNG_LAMENT)
            m_ptr->song_lament_completed = 1;
    }

    song_duel_clear_player_target();
    song_duel_reset_player_stack();

    p_ptr->song_lockout_timer = SONG_DUEL_LOCKOUT_TURNS;

    if (song == SNG_CONTEST)
    {
        // Song of Contest always drains a random stat - no resistance
        int stat = rand_int(A_MAX);
        static cptr desc_stat_neg[] = { "weak", "awkward", "sickly", "drained" };
        if (dec_stat(stat, 1, false))
            msg_format("You feel %s.", desc_stat_neg[stat]);
    }

    change_song(SNG_NOTHING);
}

bool song_duel_process_contest(int song_skill)
{
    monster_type* m_ptr = song_duel_get_target(SNG_CONTEST);
    if (!m_ptr)
    {
        msg_print("Your contest has no opponent.");
        change_song(SNG_NOTHING);
        return false;
    }

    char m_name[80];
    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

    int player_skill = song_skill + (p_ptr->skill_use[S_WIL] / 2);
    int monster_will = monster_skill(m_ptr, S_WIL);

    int result = skill_check(PLAYER, player_skill, monster_will, m_ptr);

    if (result > 0)
    {
        p_ptr->song_contest_player_stacks = 0;
        p_ptr->song_contest_last_turn = playerturn;

        if (m_ptr->song_contest_stacks < SONG_DUEL_STACK_LIMIT)
            m_ptr->song_contest_stacks++;
        m_ptr->song_contest_last_turn = playerturn;

        if (m_ptr->ml)
            msg_format("%s falters under your contest. (%d/%d)", m_name,
                m_ptr->song_contest_stacks, SONG_DUEL_STACK_LIMIT);
        else
            msg_print("You press your advantage in the contest.");

        if (m_ptr->song_contest_stacks >= SONG_DUEL_STACK_LIMIT)
        {
            song_duel_finish_monster_loss(m_ptr, SNG_CONTEST, song_skill);
            return false;
        }
    }
    else if (result < 0)
    {
        m_ptr->song_contest_stacks = 0;
        m_ptr->song_contest_last_turn = playerturn;

        if (p_ptr->song_contest_player_stacks < SONG_DUEL_STACK_LIMIT)
            p_ptr->song_contest_player_stacks++;
        p_ptr->song_contest_last_turn = playerturn;

        msg_print("Your foe pushes back against your song.");

        if (p_ptr->song_contest_player_stacks >= SONG_DUEL_STACK_LIMIT)
        {
            song_duel_finish_player_loss(SNG_CONTEST, m_ptr);
            return false;
        }
    }
    else
    {
        msg_print("The contest hangs in the balance.");
    }

    return true;
}

bool song_duel_process_lament(int song_skill)
{
    monster_type* m_ptr = song_duel_get_target(SNG_LAMENT);
    if (!m_ptr)
    {
        msg_print("Your lament has no audience.");
        change_song(SNG_NOTHING);
        return false;
    }

    char m_name[80];
    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

    int player_skill = song_skill + (p_ptr->skill_use[S_WIL] / 2);
    int monster_will = monster_skill(m_ptr, S_WIL);

    int result = skill_check(PLAYER, player_skill, monster_will, m_ptr);

    if (result > 0)
    {
        if (m_ptr->song_lament_stacks < SONG_DUEL_STACK_LIMIT)
            m_ptr->song_lament_stacks++;
        m_ptr->song_lament_last_turn = playerturn;

        if (m_ptr->ml)
            msg_format("%s reels beneath your lament. (%d/%d)", m_name,
                m_ptr->song_lament_stacks, SONG_DUEL_STACK_LIMIT);
        else
            msg_print("Your lament burdens an unseen foe.");

        if (m_ptr->song_lament_stacks >= SONG_DUEL_STACK_LIMIT)
        {
            song_duel_finish_monster_loss(m_ptr, SNG_LAMENT, song_skill);
            return false;
        }
    }
    else if (result < 0)
    {
        m_ptr->song_lament_stacks = 0;
        m_ptr->song_lament_last_turn = playerturn;
        msg_print("Your lament fails to take hold.");
    }
    else
    {
        msg_print("Your lament and the foe's will are evenly matched.");
    }

    return true;
}

void song_duels_new_player_turn(void)
{
    if (p_ptr->song_lockout_timer > 0)
        p_ptr->song_lockout_timer--;

    if (p_ptr->song_contest_player_stacks > 0
        && p_ptr->song_contest_last_turn > 0
        && (playerturn - p_ptr->song_contest_last_turn) >= SONG_DUEL_LOCKOUT_TURNS)
    {
        song_duel_reset_player_stack();
    }

    for (int i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];
        if (!m_ptr->r_idx)
            continue;

        if (m_ptr->song_lockout_timer > 0)
            m_ptr->song_lockout_timer--;

        if (m_ptr->song_contest_stacks > 0
            && m_ptr->song_contest_last_turn > 0
            && (playerturn - m_ptr->song_contest_last_turn) >= SONG_DUEL_LOCKOUT_TURNS)
        {
            m_ptr->song_contest_stacks = 0;
            m_ptr->song_contest_last_turn = 0;
        }

        if (m_ptr->song_lament_stacks > 0
            && m_ptr->song_lament_last_turn > 0
            && (playerturn - m_ptr->song_lament_last_turn) >= SONG_DUEL_LOCKOUT_TURNS)
        {
            m_ptr->song_lament_stacks = 0;
            m_ptr->song_lament_last_turn = 0;
        }
    }
}

void song_duels_handle_monster_removed(int m_idx)
{
    if (p_ptr->song_target_idx == m_idx)
    {
        song_duel_clear_player_target();
        song_duel_reset_player_stack();
    }
}

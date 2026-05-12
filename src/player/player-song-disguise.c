/* File: player/player-song-disguise.c */
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
#include "player/player-song-disguise.h"

bool song_disguise_active = false;
static byte* song_disguise_seen = NULL;
static byte* song_disguise_pacified = NULL;
static byte* song_disguise_attacked = NULL;
static int song_disguise_seen_count = 0;
static int song_disguise_attackers_current_turn = 0;
static int song_disguise_attackers_last_turn = 0;

byte song_revealing_hint[MAX_MONSTERS];  // Stores detection result quality
bool song_revealing_has_data = false;

#define SONG_REVEALING_HINT_TTL 3
#ifndef SONG_REVEALING_FULL_VISIBILITY
#define SONG_REVEALING_FULL_VISIBILITY 10  // Threshold for full visibility
#endif

static void ensure_song_disguise_buffers(void)
{
    if (!song_disguise_seen)
    {
        song_disguise_seen = mem_alloc_array(MAX_MONSTERS, byte);
        song_disguise_pacified = mem_alloc_array(MAX_MONSTERS, byte);
        song_disguise_attacked = mem_alloc_array(MAX_MONSTERS, byte);
    }
}

static void song_disguise_clear_pacified(void)
{
    if (!song_disguise_pacified)
        return;

    memset(song_disguise_pacified, 0, MAX_MONSTERS * sizeof(byte));
}

void song_disguise_on_start(void)
{
    ensure_song_disguise_buffers();
    song_disguise_clear_pacified();
    song_disguise_active = true;
}

void song_disguise_on_stop(void)
{
    song_disguise_active = false;
    song_disguise_clear_pacified();
    song_disguise_attackers_current_turn = 0;
}

static bool monster_currently_sees_player(const monster_type* m_ptr)
{
    if (m_ptr->alertness < ALERTNESS_ALERT)
        return false;

    if (!los(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px))
        return false;

    const monster_race* r_ptr = &r_info[m_ptr->r_idx];
    if (r_ptr->flags1 & RF1_PEACEFUL)
        return false;

    return true;
}

bool any_monster_observes_player(void)
{
    for (int i = mon_max - 1; i >= 1; i--)
    {
        monster_type* m_ptr = &mon_list[i];

        if (!m_ptr->r_idx)
            continue;

        if (monster_currently_sees_player(m_ptr))
            return true;
    }

    return false;
}

static int count_monsters_observing_player(void)
{
    int count = 0;

    for (int i = mon_max - 1; i >= 1; i--)
    {
        monster_type* m_ptr = &mon_list[i];

        if (!m_ptr->r_idx)
            continue;

        if (monster_currently_sees_player(m_ptr))
            count++;
    }

    return count;
}

void song_disguise_new_player_turn(void)
{
    ensure_song_disguise_buffers();

    song_disguise_attackers_last_turn = song_disguise_attackers_current_turn;
    song_disguise_attackers_current_turn = 0;

    if (song_disguise_attacked)
        memset(song_disguise_attacked, 0, MAX_MONSTERS * sizeof(byte));
}

void song_disguise_handle_monster_removed(int m_idx)
{
    if (!song_disguise_seen || m_idx <= 0 || m_idx >= MAX_MONSTERS)
        return;

    if (song_disguise_seen[m_idx])
    {
        song_disguise_seen[m_idx] = 0;
        if (song_disguise_seen_count > 0)
            song_disguise_seen_count--;
    }

    if (song_disguise_pacified)
        song_disguise_pacified[m_idx] = 0;
    if (song_disguise_attacked)
        song_disguise_attacked[m_idx] = 0;

    if (m_idx > 0 && m_idx < MAX_MONSTERS)
    {
        song_revealing_hint[m_idx] = 0;

        if (song_revealing_has_data)
        {
            bool any_hint = false;
            for (int i = 1; i < MAX_MONSTERS; i++)
            {
                if (song_revealing_hint[i])
                {
                    any_hint = true;
                    break;
                }
            }

            if (!any_hint)
                song_revealing_has_data = false;
        }
    }
}

void song_disguise_note_monster_attack(int m_idx)
{
    if (m_idx <= 0)
        return;

    ensure_song_disguise_buffers();

    if (!song_disguise_attacked[m_idx])
    {
        song_disguise_attacked[m_idx] = 1;
        song_disguise_attackers_current_turn++;
    }
}

void song_disguise_note_player_attack(int m_idx)
{
    (void)m_idx;

    if (!singing(SNG_DISGUISE))
        return;

    if (p_ptr->song1 == SNG_DISGUISE)
    {
        if (p_ptr->song2 != SNG_NOTHING)
        {
            p_ptr->song1 = p_ptr->song2;
            p_ptr->song2 = SNG_NOTHING;
            msg_print("Your attack breaks your song of disguise.");
        }
        else
        {
            p_ptr->song1 = SNG_NOTHING;
            msg_print("Your attack ends your song of disguise.");
        }
    }
    else if (p_ptr->song2 == SNG_DISGUISE)
    {
        p_ptr->song2 = SNG_NOTHING;
        msg_print("Your attack ends your minor theme of disguise.");
    }

    song_disguise_on_stop();

    p_ptr->redraw |= (PR_SONG);
    p_ptr->update |= (PU_BONUS);
}

void song_revealing_decay(void)
{
    bool any = false;

    for (int i = 1; i < MAX_MONSTERS; i++)
    {
        if (song_revealing_hint[i] > 0)
        {
            // Decay the detection quality each turn (reduce by ~3-4 points per turn)
            if (song_revealing_hint[i] > 3)
                song_revealing_hint[i] -= 3;
            else
                song_revealing_hint[i] = 0;
        }

        if (song_revealing_hint[i] > 0)
            any = true;
    }

    song_revealing_has_data = any;
}

bool song_revealing_overlay(int m_idx, byte* a, char* c)
{
    if (!song_revealing_has_data)
        return false;

    if (m_idx <= 0 || m_idx >= MAX_MONSTERS)
        return false;

    if (!song_revealing_hint[m_idx])
        return false;

    monster_type* m_ptr = &mon_list[m_idx];

    if (!m_ptr->r_idx || m_ptr->ml)
        return false;

    // If detection quality is high enough, make the monster fully visible
    if (song_revealing_hint[m_idx] > SONG_REVEALING_FULL_VISIBILITY)
    {
        m_ptr->ml = true;
        return false;  // Let normal rendering handle it
    }

    // Otherwise show as a hint marker
    if (graphics_are_ascii())
    {
        int base = 0x30;
        int k = TERM_SLATE;
        byte idx = (byte)(base + k);
        *a = misc_to_attr[idx];
        *c = misc_to_char[idx];
    }
    else
    {
        *a = misc_to_attr[ICON_UNKNOWN_ENEMY];
        *c = misc_to_char[ICON_UNKNOWN_ENEMY];
    }

    return true;
}


bool song_disguise_monster_is_fooled(const monster_type* m_ptr)
{
    if (!song_disguise_active)
        return false;

    if (!song_disguise_pacified)
        return false;

    int m_idx = cave_m_idx[m_ptr->fy][m_ptr->fx];

    if (m_idx <= 0 || m_idx >= MAX_MONSTERS)
        return false;

    if (!song_disguise_pacified[m_idx])
        return false;

    if (!monster_currently_sees_player(m_ptr))
        return false;

    return true;
}

void sing_song_of_disguise(int score)
{
    ensure_song_disguise_buffers();

    song_disguise_clear_pacified();

    int player_skill = score;
    int observer_penalty = count_monsters_observing_player();

    // Turgon's unique: Shadow Walker - add Perception to the check
    if (c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_TURGON)
    {
        player_skill += p_ptr->skill_use[S_PER];
    }

    for (int i = mon_max - 1; i >= 1; i--)
    {
        monster_type* m_ptr = &mon_list[i];
        char m_name[80];

        if (!m_ptr->r_idx)
            continue;

        if (!monster_currently_sees_player(m_ptr))
            continue;

        int difficulty = monster_skill(m_ptr, S_WIL)
            + monster_skill(m_ptr, S_PER);

        difficulty += observer_penalty;

        if (m_ptr->cdis > 1)
            difficulty -= (m_ptr->cdis - 1);

        if (difficulty < 0)
            difficulty = 0;

        int m_idx = i;

        int other_watchers = song_disguise_seen_count;
        if (song_disguise_seen[m_idx])
            other_watchers--;
        if (other_watchers > 0)
            difficulty += other_watchers * 5;

        if (song_disguise_attackers_last_turn > 0)
            difficulty += song_disguise_attackers_last_turn * 5;

        if (song_disguise_seen[m_idx])
            difficulty += 10;

        int result = skill_check(
            PLAYER, player_skill, difficulty, m_ptr);

        if (result > 0)
        {
            song_disguise_pacified[m_idx] = 1;
            if (song_disguise_seen[m_idx])
            {
                song_disguise_seen[m_idx] = 0;
                if (song_disguise_seen_count > 0)
                    song_disguise_seen_count--;
            }
        }
        else
        {
            if (!song_disguise_seen[m_idx])
            {
                monster_desc(m_name, sizeof(m_name), m_ptr, 0);
                msg_format("%^s sees through your disguise.", m_name);
                song_disguise_seen[m_idx] = 1;
                song_disguise_seen_count++;
            }
        }
    }
}

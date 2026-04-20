/* File: player/player-songs.c */
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

#include "player/player-songs.h"

int affinity_level(int skilltype)
{
    int  level         = 0;
    u32b affinity_flag = 0L;
    u32b penalty_flag  = 0L;

    /* map skill → (affinity, penalty) pair */
    switch (skilltype)
    {
        case S_MEL: affinity_flag = RHF_MEL_AFFINITY; penalty_flag = RHF_MEL_PENALTY; break;
        case S_ARC: affinity_flag = RHF_ARC_AFFINITY; penalty_flag = RHF_ARC_PENALTY; break;
        case S_EVN: affinity_flag = RHF_EVN_AFFINITY; penalty_flag = RHF_EVN_PENALTY; break;
        case S_STL: affinity_flag = RHF_STL_AFFINITY; penalty_flag = RHF_STL_PENALTY; break;
        case S_PER: affinity_flag = RHF_PER_AFFINITY; penalty_flag = RHF_PER_PENALTY; break;
        case S_WIL: affinity_flag = RHF_WIL_AFFINITY; penalty_flag = RHF_WIL_PENALTY; break;
        case S_SMT: affinity_flag = RHF_SMT_AFFINITY; penalty_flag = RHF_SMT_PENALTY; break;
        case S_SNG: affinity_flag = RHF_SNG_AFFINITY; penalty_flag = RHF_SNG_PENALTY; break;
        default:    return 0;
    }

    /* race + character */
    if (rp_ptr->flags & affinity_flag) level++;
    if (current_character_profile->flags & affinity_flag) level++;
    if (rp_ptr->flags & penalty_flag)  level--;
    if (current_character_profile->flags & penalty_flag)  level--;

    /* every copy of the same curse flag */
    level += curse_flag_count_rhf(affinity_flag);
    level -= curse_flag_count_rhf(penalty_flag);

    /* keep inside the allowed range */
    if (level >  2) level =  2;
    if (level < -2) level = -2;

    if ((skilltype == S_WIL) && (current_character_profile->flags_u & UNQ_EARENDIL)) level = 3;

    return level;
}

/*
 * Calculate the minstrel bonus for song abilities.
 * Unlike affinity_level, this is uncapped and only affects ability costs.
 * It does not provide skill increases.
 */
int minstrel_level(void)
{
    int level = 0;

    /* Check for MINSTREL unique flag */
    if (current_character_profile->flags_u & UNQ_MINSTREL) level++;

    /* Include curse flags (similar to affinity) */
    level += curse_flag_count_rhf(RHF_SNG_AFFINITY);
    level -= curse_flag_count_rhf(RHF_SNG_PENALTY);

    /* No cap - can go beyond 2 */
    return level;
}

static bool songs_are_synergy_pair(byte song_a, byte song_b)
{
    static const byte synergy_pairs[][2] = {
        { SNG_ELBERETH,  SNG_TREES },
        { SNG_ELBERETH,  SNG_STAUNCHING },
        { SNG_CHALLENGE, SNG_SLAYING },
        { SNG_DELVINGS,  SNG_REVEALING },
        { SNG_FREEDOM,   SNG_ELVENESS },
        { SNG_STAYING,   SNG_CONTEST },
        { SNG_STAYING,   SNG_LAMENT },
        { SNG_SILENCE,   SNG_DISGUISE },
        { SNG_SILENCE,   SNG_LORIEN },
        { SNG_SHATTERING, SNG_MASTERY },
    };

    if ((song_a == SNG_NOTHING) || (song_b == SNG_NOTHING))
        return false;

    for (size_t i = 0; i < N_ELEMENTS(synergy_pairs); i++)
    {
        if ((song_a == synergy_pairs[i][0] && song_b == synergy_pairs[i][1])
            || (song_a == synergy_pairs[i][1] && song_b == synergy_pairs[i][0]))
        {
            return true;
        }
    }

    return false;
}

static int song_synergy_bonus(byte abilitynum, int full_skill)
{
    int synergy = 0;
    byte partner = SNG_NOTHING;

    if (full_skill <= 0)
        return 0;

    if (p_ptr->song1 == abilitynum)
        partner = p_ptr->song2;
    else if (p_ptr->song2 == abilitynum)
        partner = p_ptr->song1;
    else
        return 0;

    if (!songs_are_synergy_pair(abilitynum, partner))
        return 0;

    /* 10% of base song skill (integer math, rounded). */
    synergy = (full_skill + 5) / 10;

    return synergy;
}

int song_effective_skill(int abilitynum)
{
    int skill = p_ptr->skill_use[S_SNG];
    const int full_skill = skill;

    // penalize minor themes - check if this ability is the minor theme
    // UNLESS the character has the WOVEN_MASTER flag (Daeron)
    if ((p_ptr->song2 == abilitynum) && (p_ptr->song1 != abilitynum))
    {
        if (!(c_info[p_ptr->pcharacter].flags_u & UNQ_WOVEN_MASTER))
            skill /= 2;
    }

    // Song of Silence dampens other songs when woven together
    // EXCEPT for Disguise and Lorien (its synergy pairs)
    // This dampening is applied BEFORE synergy bonus
    if (singing(SNG_SILENCE) && (abilitynum != SNG_SILENCE)
        && (abilitynum != SNG_DISGUISE) && (abilitynum != SNG_LORIEN))
    {
        // Calculate Silence bonus directly to avoid recursion
        int silence_skill = p_ptr->skill_use[S_SNG] / 2;
        int silence_penalty = silence_skill / 2;
        skill -= silence_penalty;
        if (skill < 0)
            skill = 0;
    }

    // woven theme synergy pairs grant an extra 20% of base song skill
    skill += song_synergy_bonus(abilitynum, full_skill);

    // effective skill is never negative
    if (skill < 0)
        skill = 0;

    return skill;
}

int ability_bonus(int skilltype, int abilitynum)
{
    int bonus = 0;
    int skill = p_ptr->skill_use[skilltype];

    if (skilltype == S_SNG)
    {
        skill = song_effective_skill(abilitynum);

        switch (abilitynum)
        {
        case SNG_ELBERETH:
        {
            bonus = skill;
            break;
        }
        case SNG_CHALLENGE:
        {
            bonus = skill;
            break;
        }
        case SNG_FREEDOM:
        {
            bonus = skill;
            break;
        }
        case SNG_STAUNCHING:
        {
            bonus = skill;
            break;
        }
        case SNG_SILENCE:
        {
            bonus = skill / 2;
            break;
        }
        case SNG_DELVINGS:
        {
            bonus = skill;
            break;
        }
        case SNG_REVEALING:
        {
            bonus = skill;
            break;
        }
        case SNG_THRESHOLDS:
        {
            bonus = skill;
            break;
        }
        case SNG_TREES:
        {
            bonus = skill / 5;
            break;
        }
        case SNG_ELVENESS:
        {
            bonus = skill;
            break;
        }
        case SNG_DISGUISE:
        {
            bonus = skill + 5;
            break;
        }
        case SNG_STAYING:
        {
            bonus = ((c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_FIN) ? 2 : 1) * skill; 
            break;
        }
        case SNG_SLAYING:
        {
            bonus = ((c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_HURIN) ? 2 : 1) * skill * 2;
            break;
        }
        case SNG_LORIEN:
        {
            bonus = skill;
            break;
        }
        case SNG_MASTERY:
        {
            /* Thingol: Song of Mastery is 1.75x effective (7/4 as integer math) */
            bonus = ((c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_THINGOL) ? (7 * skill) / 4 : skill);
            break;
        }
        case SNG_SHATTERING:
        {
            bonus = skill;
            break;
        }
        case SNG_CONTEST:
        {
            bonus = skill;
            break;
        }
        case SNG_LAMENT:
        {
            bonus = skill;
            break;
        }
        }

        // these bonuses are never negative
        if (bonus < 0)
            bonus = 0;
    }

    return (bonus);
}

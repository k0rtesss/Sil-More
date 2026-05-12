/* File: player/player-abilities.c */
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

#include "player/player-abilities.h"

static byte ability_requirement_suspended[S_MAX][ABILITIES_MAX];

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

        if (b_ptr->hidden)
            continue;

        if (b_ptr->knowledge_cost > 0)
            continue;

        if (p_ptr->innate_ability[skilltype][b_ptr->abilitynum])
            count++;
    }

    return count;
}

static ability_type* ability_info_for(int skilltype, int abilitynum)
{
    ability_type* b_ptr;

    if (!z_info || !b_info)
        return NULL;
    if (skilltype < 0 || skilltype >= S_MAX)
        return NULL;
    if (abilitynum < 0 || abilitynum >= ABILITIES_MAX)
        return NULL;

    b_ptr = &b_info[ability_index(skilltype, abilitynum)];
    if (!b_ptr->name || b_ptr->skilltype != skilltype
        || b_ptr->abilitynum != abilitynum)
    {
        return NULL;
    }

    return b_ptr;
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
    ability_type* b_ptr = ability_info_for(skilltype, abilitynum);

    return b_ptr && prereq_abilities_met(b_ptr);
}

int ability_requirement_level(const ability_type* b_ptr)
{
    int level;
    int i;

    if (!b_ptr)
        return 0;

    level = b_ptr->level;
    if (b_ptr->lore_req > level)
        level = b_ptr->lore_req;

    for (i = 0; i < A_MAX; i++)
        if (b_ptr->stat_req[i] > level)
            level = b_ptr->stat_req[i];

    for (i = 0; i < S_MAX; i++)
        if (b_ptr->skill_req[i] > level)
            level = b_ptr->skill_req[i];

    return level;
}

static int ability_requirement_current_stat_value(int stat)
{
    if (!p_ptr || stat < 0 || stat >= A_MAX)
        return 0;

    return p_ptr->stat_base[stat] + p_ptr->stat_drain[stat]
        + p_ptr->stat_equip_mod[stat];
}

static int ability_requirement_current_skill_value(int skill)
{
    if (!p_ptr || skill < 0 || skill >= S_MAX)
        return 0;

    return p_ptr->skill_base[skill] + p_ptr->skill_equip_mod[skill];
}

static bool ability_has_runtime_requirements(const ability_type* b_ptr)
{
    int i;

    if (!b_ptr)
        return false;

    if (b_ptr->lore_req > 0 || b_ptr->lore_req_lt > 0)
        return true;

    for (i = 0; i < A_MAX; i++)
    {
        if (b_ptr->stat_req[i] > 0 || b_ptr->stat_req_lt[i] > 0)
            return true;
    }

    for (i = 0; i < S_MAX; i++)
    {
        if (b_ptr->skill_req[i] > 0 || b_ptr->skill_req_lt[i] > 0)
            return true;
    }

    return false;
}

bool ability_requirements_currently_met(int skilltype, int abilitynum)
{
    ability_type* b_ptr = ability_info_for(skilltype, abilitynum);
    int i;

    if (!b_ptr)
        return false;

    if ((b_ptr->lore_req > 0) && (b_ptr->lore_req > p_ptr->lore))
        return false;

    if ((b_ptr->lore_req_lt > 0) && (p_ptr->lore >= b_ptr->lore_req_lt))
        return false;

    for (i = 0; i < A_MAX; i++)
    {
        if ((b_ptr->stat_req[i] > 0)
            && (b_ptr->stat_req[i] > ability_requirement_current_stat_value(i)))
        {
            return false;
        }
        if ((b_ptr->stat_req_lt[i] > 0)
            && (ability_requirement_current_stat_value(i)
                >= b_ptr->stat_req_lt[i]))
        {
            return false;
        }
    }

    for (i = 0; i < S_MAX; i++)
    {
        if ((b_ptr->skill_req[i] > 0)
            && (b_ptr->skill_req[i]
                > ability_requirement_current_skill_value(i)))
        {
            return false;
        }
        if ((b_ptr->skill_req_lt[i] > 0)
            && (ability_requirement_current_skill_value(i)
                >= b_ptr->skill_req_lt[i]))
        {
            return false;
        }
    }

    return true;
}

bool object_grants_usable_ability(
    const object_type* o_ptr, int skilltype, int abilitynum)
{
    return object_grants_ability(o_ptr, skilltype, abilitynum)
        && ability_requirements_currently_met(skilltype, abilitynum);
}

bool ability_requirement_is_suspended(int skilltype, int abilitynum)
{
    if (skilltype < 0 || skilltype >= S_MAX)
        return false;
    if (abilitynum < 0 || abilitynum >= ABILITIES_MAX)
        return false;

    return ability_requirement_suspended[skilltype][abilitynum] ? true : false;
}

static void stop_song_if_ability_suspended(int skilltype, int abilitynum)
{
    if (skilltype != S_SNG)
        return;

    if (p_ptr->song1 == abilitynum)
    {
        p_ptr->song1 = p_ptr->song2;
        p_ptr->song2 = SNG_NOTHING;
        p_ptr->redraw |= PR_SONG;
    }
    else if (p_ptr->song2 == abilitynum)
    {
        p_ptr->song2 = SNG_NOTHING;
        p_ptr->redraw |= PR_SONG;
    }

    if (abilitynum == SNG_WOVEN_THEMES && p_ptr->song2 != SNG_NOTHING)
    {
        p_ptr->song2 = SNG_NOTHING;
        p_ptr->redraw |= PR_SONG;
    }
}

void update_active_ability_requirements(void)
{
    bool changed = false;

    if (!p_ptr || !z_info || !b_info)
        return;

    for (int i = 0; i < z_info->b_max; i++)
    {
        ability_type* b_ptr = &b_info[i];
        int skilltype;
        int abilitynum;
        bool met;

        if (!b_ptr->name)
            continue;

        skilltype = b_ptr->skilltype;
        abilitynum = b_ptr->abilitynum;

        if (skilltype < 0 || skilltype >= S_MAX
            || abilitynum < 0 || abilitynum >= ABILITIES_MAX)
        {
            continue;
        }

        if (!p_ptr->have_ability[skilltype][abilitynum])
        {
            ability_requirement_suspended[skilltype][abilitynum] = false;
            continue;
        }

        if (!ability_has_runtime_requirements(b_ptr))
        {
            ability_requirement_suspended[skilltype][abilitynum] = false;
            continue;
        }

        met = ability_requirements_currently_met(skilltype, abilitynum);

        if (p_ptr->active_ability[skilltype][abilitynum] && !met)
        {
            p_ptr->active_ability[skilltype][abilitynum] = false;
            ability_requirement_suspended[skilltype][abilitynum] = true;
            stop_song_if_ability_suspended(skilltype, abilitynum);
            changed = true;
        }
        else if (!p_ptr->active_ability[skilltype][abilitynum]
            && ability_requirement_suspended[skilltype][abilitynum] && met)
        {
            p_ptr->active_ability[skilltype][abilitynum] = true;
            ability_requirement_suspended[skilltype][abilitynum] = false;
            changed = true;
        }
    }

    if (changed)
    {
        p_ptr->redraw |= (PR_EXP | PR_BASIC);
        p_ptr->window |= (PW_PLAYER_0);
    }
}

bool prereqs(int skilltype, int abilitynum)
{
    if (!ability_requirements_currently_met(skilltype, abilitynum))
        return false;

    return ability_prereqs_met(skilltype, abilitynum);
}

static int ability_score_apply_weight(int value, int weight)
{
    long scaled = (long)value * (long)weight;

    if (scaled >= 0)
        return (int)((scaled + 50L) / 100L);

    return (int)(-((-scaled + 50L) / 100L));
}

bool ability_score_has_custom_weights(int skilltype, int abilitynum)
{
    ability_type* b_ptr = ability_info_for(skilltype, abilitynum);

    return b_ptr && b_ptr->score_weights_set;
}

static int ability_score_skill_source(int skilltype)
{
    if (!p_ptr || skilltype < 0 || skilltype >= S_MAX)
        return 0;

    return p_ptr->skill_base[skilltype] + p_ptr->skill_equip_mod[skilltype]
        + p_ptr->skill_misc_mod[skilltype];
}

int ability_score(int skilltype, int abilitynum)
{
    ability_type* b_ptr = ability_info_for(skilltype, abilitynum);
    int score = 0;

    if (!p_ptr)
        return 0;

    if (!b_ptr || !b_ptr->score_weights_set)
    {
        if (skilltype >= 0 && skilltype < S_MAX)
            return p_ptr->skill_use[skilltype];

        return 0;
    }

    for (int i = 0; i < A_MAX; i++)
    {
        if (b_ptr->stat_score_weight_set[i])
        {
            score += ability_score_apply_weight(
                p_ptr->stat_use[i], b_ptr->stat_score_weight[i]);
        }
    }

    for (int i = 0; i < S_MAX; i++)
    {
        if (b_ptr->skill_score_weight_set[i])
        {
            score += ability_score_apply_weight(
                ability_score_skill_source(i), b_ptr->skill_score_weight[i]);
        }
    }

    return score;
}

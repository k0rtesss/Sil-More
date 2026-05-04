/* File: metarun-monster-events.c */
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
#include "metarun/metarun-meta-state.h"
#include "player/killer.h"
#include "score/score_guid.h"

bool meta_monster_record_current_player_death(cptr cause)
{
    const killer_info* killer = killer_last();
    const monster_type* killer_mon = killer_last_monster();
    meta_monster_death_event event;
    int killer_r_idx;
    monster_race* killer_r_ptr;

    if (!killer || killer->kind != SCORE_KILLER_MONSTER)
        return false;

    killer_r_idx = killer_mon ? killer_mon->r_idx : killer->race_index;
    if ((killer_r_idx <= 0) || (killer_r_idx >= z_info->r_max))
        return false;

    killer_r_ptr = &r_info[killer_r_idx];
    if ((killer_r_idx == R_IDX_MORGOTH) || (killer_r_ptr->flags1 & RF1_QUESTOR))
        return false;

    memset(&event, 0, sizeof(event));
    event.monster_guid = score_guid_from_u64(killer_r_ptr->guid);
    event.r_idx = (u16b)killer_r_idx;
    SDL_strlcpy(event.monster_name,
        r_name
            + (((r_base != NULL) ? r_base[killer_r_idx].name
                                 : killer_r_ptr->name)),
        sizeof(event.monster_name));
    event.character_guid = c_info[p_ptr->pcharacter].guid;
    SDL_strlcpy(event.character_name, c_name + c_info[p_ptr->pcharacter].name,
        sizeof(event.character_name));
    event.depth = (byte)MAX(0, p_ptr->depth);
    event.turn = turn;
    SDL_strlcpy(event.cause, cause ? cause : "", sizeof(event.cause));

    return meta_monster_record_player_death(&event);
}

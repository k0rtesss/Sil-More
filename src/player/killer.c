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

#include "player/killer.h"

#include "angband.h"
#include "score/score_guid.h"

static killer_info pending_info;
static killer_info last_info;

void killer_reset(void)
{
    memset(&pending_info, 0, sizeof(pending_info));
    memset(&last_info, 0, sizeof(last_info));
}

static void killer_set_pending(score_killer_kind kind, score_guid64 guid,
                               u16b race_index)
{
    pending_info.kind = kind;
    pending_info.guid = guid;
    pending_info.race_index = race_index;
    pending_info.valid = true;
}

void killer_mark_monster(const monster_type* m_ptr)
{
    if (!m_ptr)
        return;

    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    guid64 guid = score_guid_from_u64(r_ptr->guid);
    killer_set_pending(SCORE_KILLER_MONSTER, guid, (u16b)m_ptr->r_idx);
}

void killer_mark_other(score_killer_kind kind)
{
    killer_set_pending(kind, score_guid_from_u64(0), 0);
}

void killer_commit(cptr cause)
{
    (void)cause;
    if (pending_info.valid) {
        last_info = pending_info;
        pending_info.valid = false;
    } else {
        killer_set_pending(SCORE_KILLER_OTHER, score_guid_from_u64(0), 0);
        last_info = pending_info;
        pending_info.valid = false;
    }
    last_info.valid = true;
}

const killer_info* killer_last(void)
{
    return last_info.valid ? &last_info : NULL;
}

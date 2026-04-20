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

#ifndef INCLUDED_PLAYER_KILLER_H
#define INCLUDED_PLAYER_KILLER_H

#include "h-basic.h"
#include "score/score_format.h"

struct monster_type;

typedef struct killer_info {
    bool valid;
    score_killer_kind kind;
    score_guid64 guid;
    u16b race_index;
} killer_info;

void killer_reset(void);
void killer_mark_monster(const struct monster_type* m_ptr);
void killer_mark_other(score_killer_kind kind);
void killer_commit(cptr cause);
const killer_info* killer_last(void);

#endif /* INCLUDED_PLAYER_KILLER_H */

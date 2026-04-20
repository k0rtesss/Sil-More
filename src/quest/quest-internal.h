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

#ifndef INCLUDED_QUEST_INTERNAL_H
#define INCLUDED_QUEST_INTERNAL_H

#include "h-basic.h"

int get_quest_oath_id(int quest_idx);
void grant_followup_quest_rewards(int quest_id);
void remove_quest_giver_silent(int quest_giver_r_idx);
bool tulkas_has_valid_target(int depth);

#endif /* INCLUDED_QUEST_INTERNAL_H */

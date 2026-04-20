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

#ifndef INCLUDED_PLAYER_OATHS_H
#define INCLUDED_PLAYER_OATHS_H

#include "h-basic.h"

extern char* oath_name[];

bool oath_invalid(int oath_id);
void apply_oath_breaking_curse(int oath_id);
bool chosen_oath(int oath_id);
char* oath_confirmation_prompt(int oath_id);
char* oath_curse_message(int oath_id);
char* oath_permanent_message(int oath_id);
char* oath_death_message(int oath_id);
char* oath_banned_text(int oath_id);
char* oath_name_str(int oath_id);
char* oath_description(int oath_id);
char* oath_pledge(int oath_id);
char* oath_forbidden(int oath_id);
char* oath_reward_text(int oath_id);
bool smith_oath_forbids_object(const object_type* o_ptr);
bool smith_oath_confirm_break(void);

#endif /* INCLUDED_PLAYER_OATHS_H */

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

#ifndef INCLUDED_PLAYER_PLAYER_STATE_H
#define INCLUDED_PLAYER_PLAYER_STATE_H

#include "h-basic.h"

extern const player_race* rp_ptr;
extern character_profile* current_character_profile;
extern player_other* op_ptr;
extern player_type* p_ptr;

extern s16b stealth_score;
extern bool player_attacked;
extern bool attacked_player;

#endif /* INCLUDED_PLAYER_PLAYER_STATE_H */

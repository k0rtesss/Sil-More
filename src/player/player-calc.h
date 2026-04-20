/* File: player/player-calc.h */
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

/*
 * Core player bonus/stat calculation engine.
 */

#ifndef INCLUDED_PLAYER_CALC_H
#define INCLUDED_PLAYER_CALC_H

#include "h-basic.h"

void calc_bonuses(void);
void calc_hitpoints(void);
void calc_voice(void);
void calc_stats(void);

#endif /* INCLUDED_PLAYER_CALC_H */

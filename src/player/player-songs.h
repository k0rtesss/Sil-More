/* File: player/player-songs.h */
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
 * Music/song ability helpers.
 */

#ifndef INCLUDED_PLAYER_SONGS_H
#define INCLUDED_PLAYER_SONGS_H

#include "../h-basic.h"

int affinity_level(int skilltype);
int minstrel_level(void);
int song_effective_skill(int abilitynum);
int ability_bonus(int skilltype, int abilitynum);

#endif /* INCLUDED_PLAYER_SONGS_H */

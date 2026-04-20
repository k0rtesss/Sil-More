/* File: player/player-resources.h */
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

#ifndef INCLUDED_PLAYER_RESOURCES_H
#define INCLUDED_PLAYER_RESOURCES_H

#include "../h-basic.h"

typedef struct object_type object_type;

void calc_torch(void);
bool weapon_glows(const object_type* o_ptr);
int silmarils_possessed(void);
int has_iron_crown(void);

#endif /* INCLUDED_PLAYER_RESOURCES_H */
